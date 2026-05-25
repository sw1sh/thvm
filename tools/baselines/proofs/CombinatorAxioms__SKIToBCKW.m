ProofObject["EquationalLogic", {ForAll[{\[FormalX], \[FormalY], \[FormalZ]}, 
   CombinatorS \[Application] \[FormalX] \[Application] 
      \[FormalY] \[Application] \[FormalZ] == 
    CombinatorB \[Application] (CombinatorB \[Application] 
           (CombinatorB \[Application] CombinatorW) \[Application] 
          CombinatorC) \[Application] (CombinatorB \[Application] 
         CombinatorB) \[Application] \[FormalX] \[Application] 
      \[FormalY] \[Application] \[FormalZ]], 
  ForAll[{\[FormalX], \[FormalY], \[FormalZ]}, 
   CombinatorS \[Application] \[FormalX] \[Application] 
      \[FormalY] \[Application] \[FormalZ] == 
    CombinatorB \[Application] (CombinatorB \[Application] 
          CombinatorW) \[Application] (CombinatorB \[Application] 
          CombinatorB \[Application] CombinatorC) \[Application] 
       \[FormalX] \[Application] \[FormalY] \[Application] \[FormalZ]], 
  ForAll[{\[FormalX]}, CombinatorI \[Application] \[FormalX] == 
    CombinatorW \[Application] CombinatorK \[Application] \[FormalX]]}, 
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
    \[FormalQ], \[FormalR], \[FormalS], \[FormalX], \[FormalY], \[FormalZ]}, 
  "Constants" -> {}, "Proof" -> 
   {{"Axiom", 1} -> <|"Statement" -> HoldForm[\[FormalA] == 
         CombinatorK \[Application] \[FormalA] \[Application] \[FormalB]], 
      "Proof" -> <||>|>, {"Axiom", 2} -> 
     <|"Statement" -> HoldForm[\[FormalA] == CombinatorI \[Application] 
          \[FormalA]], "Proof" -> <||>|>, {"Axiom", 3} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[Application] 
          (\[FormalB] \[Application] \[FormalC]) == 
         CombinatorB \[Application] \[FormalA] \[Application] 
           \[FormalB] \[Application] \[FormalC]], "Proof" -> <||>|>, 
    {"Axiom", 4} -> <|"Statement" -> HoldForm[
        \[FormalA] \[Application] \[FormalB] \[Application] \[FormalC] == 
         CombinatorC \[Application] \[FormalA] \[Application] 
           \[FormalC] \[Application] \[FormalB]], "Proof" -> <||>|>, 
    {"Axiom", 5} -> <|"Statement" -> HoldForm[
        \[FormalA] \[Application] \[FormalB] \[Application] \[FormalB] == 
         CombinatorW \[Application] \[FormalA] \[Application] \[FormalB]], 
      "Proof" -> <||>|>, {"Axiom", 6} -> 
     <|"Statement" -> HoldForm[\[FormalA] \[Application] 
           \[FormalB] \[Application] (\[FormalC] \[Application] 
           \[FormalB]) == CombinatorS \[Application] 
            \[FormalA] \[Application] \[FormalC] \[Application] \[FormalB]], 
      "Proof" -> <||>|>, {"Hypothesis", 1} -> 
     <|"Statement" -> HoldForm[
        CombinatorB \[Application] (CombinatorB \[Application] 
                (CombinatorB \[Application] CombinatorW) \[Application] 
               CombinatorC) \[Application] (CombinatorB \[Application] 
              CombinatorB) \[Application] \[FormalM] \[Application] 
           \[FormalN] \[Application] \[FormalO] == 
         CombinatorS \[Application] \[FormalM] \[Application] 
           \[FormalN] \[Application] \[FormalO]], "Proof" -> <||>|>, 
    {"Hypothesis", 2} -> <|"Statement" -> 
       HoldForm[CombinatorB \[Application] (CombinatorB \[Application] 
               CombinatorW) \[Application] (CombinatorB \[Application] 
               CombinatorB \[Application] CombinatorC) \[Application] 
            \[FormalP] \[Application] \[FormalQ] \[Application] \[FormalR] == 
         CombinatorS \[Application] \[FormalP] \[Application] 
           \[FormalQ] \[Application] \[FormalR]], "Proof" -> <||>|>, 
    {"Hypothesis", 3} -> <|"Statement" -> 
       HoldForm[CombinatorW \[Application] CombinatorK \[Application] 
          \[FormalS] == CombinatorI \[Application] \[FormalS]], 
      "Proof" -> <||>|>, {"SubstitutionLemma", 1} -> 
     <|"Statement" -> HoldForm[CombinatorW \[Application] 
           CombinatorK \[Application] \[FormalS] == \[FormalS]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 3}, "Position" -> {}, 
        "Construct" -> {"Axiom", 2}, "Orientation" -> -1, 
        "Rule" -> CombinatorI \[Application] (\[FormalA]_) -> \[FormalA], 
        "OutputExpression" -> HoldForm[CombinatorW \[Application] 
             CombinatorK \[Application] \[FormalS] == \[FormalS]], 
        "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 2} -> 
     <|"Statement" -> HoldForm[CombinatorK \[Application] 
           \[FormalS] \[Application] \[FormalS] == \[FormalS]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 1}, "Position" -> {}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> CombinatorW \[Application] (\[FormalA]_) \[Application] 
           (\[FormalB]_) -> \[FormalA] \[Application] 
            \[FormalB] \[Application] \[FormalB], "OutputExpression" -> 
         HoldForm[CombinatorK \[Application] \[FormalS] \[Application] 
            \[FormalS] == \[FormalS]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"Conclusion", 1} -> <|"Statement" -> HoldForm[\[FormalS] == \[FormalS]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 2}, "Position" -> {}, 
        "Construct" -> {"Axiom", 1}, "Orientation" -> -1, 
        "Rule" -> CombinatorK \[Application] (\[FormalA]_) \[Application] 
           (\[FormalB]_) -> \[FormalA], "OutputExpression" -> 
         HoldForm[\[FormalS] == \[FormalS]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 3} -> 
     <|"Statement" -> HoldForm[
        CombinatorB \[Application] (CombinatorB \[Application] 
               CombinatorW) \[Application] CombinatorC \[Application] 
            (CombinatorB \[Application] CombinatorB \[Application] 
             \[FormalM]) \[Application] \[FormalN] \[Application] 
          \[FormalO] == CombinatorS \[Application] \[FormalM] \[Application] 
           \[FormalN] \[Application] \[FormalO]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Position" -> {1, 1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> CombinatorB \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] (\[FormalB] \[Application] \[FormalC]), 
        "OutputExpression" -> HoldForm[
          CombinatorB \[Application] (CombinatorB \[Application] 
                 CombinatorW) \[Application] CombinatorC \[Application] 
              (CombinatorB \[Application] CombinatorB \[Application] 
               \[FormalM]) \[Application] \[FormalN] \[Application] 
            \[FormalO] == CombinatorS \[Application] 
              \[FormalM] \[Application] \[FormalN] \[Application] 
            \[FormalO]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 4} -> 
     <|"Statement" -> HoldForm[
        CombinatorB \[Application] CombinatorW \[Application] 
            (CombinatorC \[Application] (CombinatorB \[Application] 
               CombinatorB \[Application] \[FormalM])) \[Application] 
           \[FormalN] \[Application] \[FormalO] == 
         CombinatorS \[Application] \[FormalM] \[Application] 
           \[FormalN] \[Application] \[FormalO]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 3}, "Position" -> {1, 1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> CombinatorB \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] (\[FormalB] \[Application] \[FormalC]), 
        "OutputExpression" -> HoldForm[
          CombinatorB \[Application] CombinatorW \[Application] 
              (CombinatorC \[Application] (CombinatorB \[Application] 
                 CombinatorB \[Application] \[FormalM])) \[Application] 
             \[FormalN] \[Application] \[FormalO] == 
           CombinatorS \[Application] \[FormalM] \[Application] 
             \[FormalN] \[Application] \[FormalO]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 5} -> 
     <|"Statement" -> HoldForm[CombinatorW \[Application] 
           (CombinatorC \[Application] (CombinatorB \[Application] 
               CombinatorB \[Application] \[FormalM]) \[Application] 
            \[FormalN]) \[Application] \[FormalO] == 
         CombinatorS \[Application] \[FormalM] \[Application] 
           \[FormalN] \[Application] \[FormalO]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 4}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> CombinatorB \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] (\[FormalB] \[Application] \[FormalC]), 
        "OutputExpression" -> HoldForm[CombinatorW \[Application] 
             (CombinatorC \[Application] (CombinatorB \[Application] 
                 CombinatorB \[Application] \[FormalM]) \[Application] 
              \[FormalN]) \[Application] \[FormalO] == 
           CombinatorS \[Application] \[FormalM] \[Application] 
             \[FormalN] \[Application] \[FormalO]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 6} -> 
     <|"Statement" -> HoldForm[
        CombinatorC \[Application] (CombinatorB \[Application] 
               CombinatorB \[Application] \[FormalM]) \[Application] 
            \[FormalN] \[Application] \[FormalO] \[Application] \[FormalO] == 
         CombinatorS \[Application] \[FormalM] \[Application] 
           \[FormalN] \[Application] \[FormalO]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 5}, "Position" -> {}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> CombinatorW \[Application] (\[FormalA]_) \[Application] 
           (\[FormalB]_) -> \[FormalA] \[Application] 
            \[FormalB] \[Application] \[FormalB], "OutputExpression" -> 
         HoldForm[CombinatorC \[Application] (CombinatorB \[Application] 
                 CombinatorB \[Application] \[FormalM]) \[Application] 
              \[FormalN] \[Application] \[FormalO] \[Application] 
            \[FormalO] == CombinatorS \[Application] 
              \[FormalM] \[Application] \[FormalN] \[Application] 
            \[FormalO]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 7} -> 
     <|"Statement" -> HoldForm[
        CombinatorB \[Application] CombinatorB \[Application] 
             \[FormalM] \[Application] \[FormalO] \[Application] 
           \[FormalN] \[Application] \[FormalO] == 
         CombinatorS \[Application] \[FormalM] \[Application] 
           \[FormalN] \[Application] \[FormalO]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 6}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> CombinatorC \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] \[FormalB], 
        "OutputExpression" -> HoldForm[
          CombinatorB \[Application] CombinatorB \[Application] 
               \[FormalM] \[Application] \[FormalO] \[Application] 
             \[FormalN] \[Application] \[FormalO] == 
           CombinatorS \[Application] \[FormalM] \[Application] 
             \[FormalN] \[Application] \[FormalO]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, {"SubstitutionLemma", 8} -> 
     <|"Statement" -> HoldForm[CombinatorB \[Application] 
            (\[FormalM] \[Application] \[FormalO]) \[Application] 
           \[FormalN] \[Application] \[FormalO] == 
         CombinatorS \[Application] \[FormalM] \[Application] 
           \[FormalN] \[Application] \[FormalO]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 7}, "Position" -> {1, 1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> CombinatorB \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] (\[FormalB] \[Application] \[FormalC]), 
        "OutputExpression" -> HoldForm[
          CombinatorB \[Application] (\[FormalM] \[Application] 
               \[FormalO]) \[Application] \[FormalN] \[Application] 
            \[FormalO] == CombinatorS \[Application] 
              \[FormalM] \[Application] \[FormalN] \[Application] 
            \[FormalO]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 9} -> 
     <|"Statement" -> HoldForm[\[FormalM] \[Application] 
           \[FormalO] \[Application] (\[FormalN] \[Application] 
           \[FormalO]) == CombinatorS \[Application] 
            \[FormalM] \[Application] \[FormalN] \[Application] \[FormalO]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 8}, "Position" -> {}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> CombinatorB \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] (\[FormalB] \[Application] \[FormalC]), 
        "OutputExpression" -> HoldForm[\[FormalM] \[Application] 
             \[FormalO] \[Application] (\[FormalN] \[Application] 
             \[FormalO]) == CombinatorS \[Application] 
              \[FormalM] \[Application] \[FormalN] \[Application] 
            \[FormalO]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"Conclusion", 2} -> 
     <|"Statement" -> HoldForm[\[FormalM] \[Application] 
           \[FormalO] \[Application] (\[FormalN] \[Application] 
           \[FormalO]) == \[FormalM] \[Application] \[FormalO] \[Application] 
          (\[FormalN] \[Application] \[FormalO])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 9}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> -1, 
        "Rule" -> CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalM] \[Application] \[FormalO] \[Application] 
            (\[FormalN] \[Application] \[FormalO]) == 
           \[FormalM] \[Application] \[FormalO] \[Application] 
            (\[FormalN] \[Application] \[FormalO])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>, 
    {"SubstitutionLemma", 10} -> 
     <|"Statement" -> HoldForm[
        CombinatorB \[Application] CombinatorW \[Application] 
            (CombinatorB \[Application] CombinatorB \[Application] 
              CombinatorC \[Application] \[FormalP]) \[Application] 
           \[FormalQ] \[Application] \[FormalR] == 
         CombinatorS \[Application] \[FormalP] \[Application] 
           \[FormalQ] \[Application] \[FormalR]], 
      "Proof" -> <|"Input" -> {"Hypothesis", 2}, "Position" -> {1, 1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> CombinatorB \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] (\[FormalB] \[Application] \[FormalC]), 
        "OutputExpression" -> HoldForm[
          CombinatorB \[Application] CombinatorW \[Application] 
              (CombinatorB \[Application] CombinatorB \[Application] 
                CombinatorC \[Application] \[FormalP]) \[Application] 
             \[FormalQ] \[Application] \[FormalR] == 
           CombinatorS \[Application] \[FormalP] \[Application] 
             \[FormalQ] \[Application] \[FormalR]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 11} -> 
     <|"Statement" -> HoldForm[
        CombinatorB \[Application] CombinatorW \[Application] 
            (CombinatorB \[Application] (CombinatorC \[Application] 
              \[FormalP])) \[Application] \[FormalQ] \[Application] 
          \[FormalR] == CombinatorS \[Application] \[FormalP] \[Application] 
           \[FormalQ] \[Application] \[FormalR]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 10}, 
        "Position" -> {1, 1, 2}, "Construct" -> {"Axiom", 3}, 
        "Orientation" -> -1, "Rule" -> 
         CombinatorB \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] (\[FormalB] \[Application] \[FormalC]), 
        "OutputExpression" -> HoldForm[
          CombinatorB \[Application] CombinatorW \[Application] 
              (CombinatorB \[Application] (CombinatorC \[Application] 
                \[FormalP])) \[Application] \[FormalQ] \[Application] 
            \[FormalR] == CombinatorS \[Application] 
              \[FormalP] \[Application] \[FormalQ] \[Application] 
            \[FormalR]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 12} -> 
     <|"Statement" -> HoldForm[CombinatorW \[Application] 
           (CombinatorB \[Application] (CombinatorC \[Application] 
              \[FormalP]) \[Application] \[FormalQ]) \[Application] 
          \[FormalR] == CombinatorS \[Application] \[FormalP] \[Application] 
           \[FormalQ] \[Application] \[FormalR]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 11}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> CombinatorB \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] (\[FormalB] \[Application] \[FormalC]), 
        "OutputExpression" -> HoldForm[CombinatorW \[Application] 
             (CombinatorB \[Application] (CombinatorC \[Application] 
                \[FormalP]) \[Application] \[FormalQ]) \[Application] 
            \[FormalR] == CombinatorS \[Application] 
              \[FormalP] \[Application] \[FormalQ] \[Application] 
            \[FormalR]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 13} -> 
     <|"Statement" -> HoldForm[
        CombinatorB \[Application] (CombinatorC \[Application] 
              \[FormalP]) \[Application] \[FormalQ] \[Application] 
           \[FormalR] \[Application] \[FormalR] == 
         CombinatorS \[Application] \[FormalP] \[Application] 
           \[FormalQ] \[Application] \[FormalR]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 12}, "Position" -> {}, 
        "Construct" -> {"Axiom", 5}, "Orientation" -> -1, 
        "Rule" -> CombinatorW \[Application] (\[FormalA]_) \[Application] 
           (\[FormalB]_) -> \[FormalA] \[Application] 
            \[FormalB] \[Application] \[FormalB], "OutputExpression" -> 
         HoldForm[CombinatorB \[Application] (CombinatorC \[Application] 
                \[FormalP]) \[Application] \[FormalQ] \[Application] 
             \[FormalR] \[Application] \[FormalR] == 
           CombinatorS \[Application] \[FormalP] \[Application] 
             \[FormalQ] \[Application] \[FormalR]], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 1|>|>, 
    {"SubstitutionLemma", 14} -> 
     <|"Statement" -> HoldForm[CombinatorC \[Application] 
            \[FormalP] \[Application] (\[FormalQ] \[Application] 
            \[FormalR]) \[Application] \[FormalR] == 
         CombinatorS \[Application] \[FormalP] \[Application] 
           \[FormalQ] \[Application] \[FormalR]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 13}, "Position" -> {1}, 
        "Construct" -> {"Axiom", 3}, "Orientation" -> -1, 
        "Rule" -> CombinatorB \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] (\[FormalB] \[Application] \[FormalC]), 
        "OutputExpression" -> HoldForm[
          CombinatorC \[Application] \[FormalP] \[Application] 
             (\[FormalQ] \[Application] \[FormalR]) \[Application] 
            \[FormalR] == CombinatorS \[Application] 
              \[FormalP] \[Application] \[FormalQ] \[Application] 
            \[FormalR]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"SubstitutionLemma", 15} -> 
     <|"Statement" -> HoldForm[\[FormalP] \[Application] 
           \[FormalR] \[Application] (\[FormalQ] \[Application] 
           \[FormalR]) == CombinatorS \[Application] 
            \[FormalP] \[Application] \[FormalQ] \[Application] \[FormalR]], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 14}, "Position" -> {}, 
        "Construct" -> {"Axiom", 4}, "Orientation" -> -1, 
        "Rule" -> CombinatorC \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] \[FormalB], 
        "OutputExpression" -> HoldForm[\[FormalP] \[Application] 
             \[FormalR] \[Application] (\[FormalQ] \[Application] 
             \[FormalR]) == CombinatorS \[Application] 
              \[FormalP] \[Application] \[FormalQ] \[Application] 
            \[FormalR]], "ConstructSide" -> 1, "InputOrientation" -> 1, 
        "Side" -> 1|>|>, {"Conclusion", 3} -> 
     <|"Statement" -> HoldForm[\[FormalP] \[Application] 
           \[FormalR] \[Application] (\[FormalQ] \[Application] 
           \[FormalR]) == \[FormalP] \[Application] \[FormalR] \[Application] 
          (\[FormalQ] \[Application] \[FormalR])], 
      "Proof" -> <|"Input" -> {"SubstitutionLemma", 15}, "Position" -> {}, 
        "Construct" -> {"Axiom", 6}, "Orientation" -> -1, 
        "Rule" -> CombinatorS \[Application] (\[FormalA]_) \[Application] 
            (\[FormalB]_) \[Application] (\[FormalC]_) -> 
          \[FormalA] \[Application] \[FormalC] \[Application] 
           (\[FormalB] \[Application] \[FormalC]), "OutputExpression" -> 
         HoldForm[\[FormalP] \[Application] \[FormalR] \[Application] 
            (\[FormalQ] \[Application] \[FormalR]) == 
           \[FormalP] \[Application] \[FormalR] \[Application] 
            (\[FormalQ] \[Application] \[FormalR])], "ConstructSide" -> 1, 
        "InputOrientation" -> 1, "Side" -> 2|>|>}|>]
