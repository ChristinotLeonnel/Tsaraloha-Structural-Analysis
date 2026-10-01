# TSA — Système Central de Traçabilité Normative & Qualité Logicielle
## Spécification et Référence d'Architecture

> **Identifiant du document :** DOC-TSA-NORM-001  
> **Version du document :** 1.1.0  
> **Version logicielle TSA :** v0.1.0-alpha  
> **Date de publication :** 2026-10-01  
> **Statut :** Approuvé & Actif  
> **Conformité documentaire :** Conforme aux exigences formelles **IEEE Std 1063-2001 (R2007)** et **ISO/IEC/IEEE 26514:2022**.

---

## 1. Portée, Public Cible et Prérequis

### 1.1 Portée (Scope)
Le présent document définit l'architecture du système central de traçabilité normative de TSA (Tsaraloha Structural Analysis). Il formalise le lien biunivoque entre les normes internationales d'ingénierie logicielle (ISO/IEC 25010, ISO/IEC/IEEE 12207, ISO/IEC/IEEE 29119), les normes de calcul structural européennes (Eurocodes EN 1990 à EN 1999), le code source C++20 et la suite de tests automatisée.

### 1.2 Public Cible (Audience Profile)
* Développeurs logiciels et mainteneurs de TSA.
* Ingénieurs en calcul de structures et auditeurs techniques.
* Responsables Assurance Qualité (QA) et organismes de certification.

### 1.3 Prérequis (Prerequisites)
* Connaissance de base des Eurocodes structuraux (EN 1990, EN 1991, EN 1992, EN 1993, EN 1993-1-11).
* Connaissance des concepts de qualité logicielle (ISO/IEC 25010) et du pattern Command / Observer.

---

## 2. Référentiels Normatifs Applicables

| Référentiel | Domaine | Description & Rôle dans TSA |
| :--- | :--- | :--- |
| **ISO/IEC 25010:2023** | Qualité du Produit Logiciel | Caractéristiques de maintenabilité (modularité, analysabilité), de fiabilité (tolérance aux pannes) et d'adéquation fonctionnelle. |
| **ISO/IEC/IEEE 12207:2017** | Cycle de Vie Logiciel | Processus d'ingénierie ordonnés : Analyse $\to$ Conception $\to$ Implémentation $\to$ Validation $\to$ Gestion des changements. |
| **ISO/IEC/IEEE 29119** | Stratégie de Test | Conception de tests unitaires, d'intégration et de validation analytique avec couverture traçable. |
| **ISO/IEC/IEEE 29148:2018** | Ingénierie des Exigences | Spécification formelle des exigences logicielles et techniques (`REQ-*`). |
| **IEEE Std 1063-2001 (R2007)** | Documentation Utilisateur | Structure, rigueur, exactitude et indexation des manuels et aides logicielles. |
| **ISO 9001:2015** | Gestion de la Qualité | Maîtrise des processus, traçabilité des modifications et auditabilité. |
| **EN 1990 (Eurocode 0)** | Bases de calcul des structures | Combinaisons d'actions ELU fondamentales (eq. 6.10) et ELS, coefficients $\psi$. |
| **EN 1991 (Eurocode 1)** | Actions sur les structures | Poids propre volumique ($\rho \cdot g \cdot A$), charges réparties, charges d'exploitation. |
| **EN 1992 (Eurocode 2)** | Structures en béton | Propriétés normées des bétons (Tableau 3.1) et vérification du ferraillage. |
| **EN 1993 (Eurocode 3)** | Structures en acier | Nuances d'aciers (Tableau 3.1), profilés IPE/HEA/HEB/UPN, stabilité des barres. |
| **EN 1993-1-11** | Câbles et tirants | Éléments tendus en acier, module sécant d'Ernst, relaxation, rentrée de mors. |

---

## 3. Architecture du Module `src/Standards/`

Le module `src/Standards/` est intégré dans `TSA_Core` et fournit les composants centraux suivants :

```text
src/Standards/
├── NormativeTypes.h / .cpp       : Énumérations des cadres normatifs, domaines et statuts d'exigences
├── RequirementsCatalog.h / .cpp  : Registre singleton des exigences (REQ-*), export Markdown et HTML
├── NationalAnnexConfig.h / .cpp  : Paramètres configurables des Annexes Nationales (France NF, DIN, BS, CEN)
├── ExternalLibraryCatalog.h/.cpp : Répertoire documenté des dépendances tierces (Qt, OCCT, OpenSees)
├── DataDefinition.h              : Métadonnées normatives des grandeurs physiques (fck, fy, E, nu, rho)
├── ModelValidator.h / .cpp       : Validateur global défensif (NaN, infini, dimensions, appuis, stabilité)
└── AnalyticalBenchmark.h / .cpp  : Registre de benchmarks et cas de référence analytiques (V&V)
```

### 3.1 Registre Central des Exigences (`RequirementsCatalog`)

Permet d'interroger et d'exporter dynamiquement la traçabilité :
```cpp
#include "Standards/RequirementsCatalog.h"

auto& catalog = TSA::Standards::RequirementsCatalog::instance();
auto req = catalog.findById("REQ-CALC-EC0-001");
if (req) {
    std::cout << req->standardTitle << " : " << req->requirementText << std::endl;
}

// Export dynamique de la matrice
QString markdownTable = catalog.generateMatrixMarkdown();
QString htmlReport = catalog.generateTraceabilityReportHtml();
```

### 3.2 Configuration des Annexes Nationales (`NationalAnnexConfig`)

Permet de basculer l'Annexe Nationale active et d'adapter instantanément les facteurs partiels de sécurité $\gamma$ et les coefficients de combinaison $\psi$ :
```cpp
#include "Standards/NationalAnnexConfig.h"

auto& annex = TSA::Standards::NationalAnnexConfig::instance();
annex.setAnnex(TSA::Standards::NationalAnnexCode::France_NF);

auto factors = annex.safetyFactors();
// factors.gammaG_sup = 1.35
// factors.gammaQ     = 1.50
// factors.gammaC     = 1.50
// factors.gammaS     = 1.15
```

### 3.3 Validateur Global de Modèle (`ModelValidator`)

Contrôle rigoureusement l'intégrité avant toute analyse par éléments finis :
```cpp
#include "Standards/ModelValidator.h"

TSA::Standards::ModelValidationReport report = TSA::Standards::ModelValidator::validate(model);
if (!report.isValid()) {
    for (const auto& err : report.formattedErrors()) {
        std::cerr << "[ERREUR NORMATIVE] " << err << std::endl;
    }
}
```

### 3.4 Registre des Cas de Référence Analytiques (`AnalyticalBenchmarkRegistry`)

Conformément à l'**ISO/IEC/IEEE 29119** et au principe de Vérification & Validation (V&V), les résultats numériques sont systématiquement confrontés à des solutions analytiques fermées de référence (Euler-Bernoulli, Timoshenko, Navier) avec tolérance certifiée ($\le 2\%$) :
```cpp
#include "Standards/AnalyticalBenchmark.h"

auto& reg = TSA::Standards::AnalyticalBenchmarkRegistry::instance();
// Évaluation analytique d'une poutre bi-appuyée (M_max = P*L/4)
auto bench = TSA::Standards::AnalyticalBenchmarkRegistry::evaluateBeamPointLoad(
    "EC3_STEEL_BEAM_001", 10.0 /* kN */, 5.0 /* m */, actualM /* kN.m */);
assert(bench.passed);
```

### 3.5 Métadonnées d'Exécution et Traçabilité des Résultats (`AnalysisExecutionMetadata`)

Chaque instance de `ResultsModel` conserve les métadonnées certifiées d'exécution :
* Annexe Nationale active (`NationalAnnexConfig`) ;
* Version du solveur OpenSees ;
* Référentiel réglementaire et type de combinaison ELU/ELS ;
* Statut d'équilibre statique global et résidu de fermeture maximal $\max |R_{unbalanced}|$ ;
* Horodatage ISO 8601 et nombre d'entités calculées.

Ces métadonnées sont automatiquement répercutées dans la Note de Calcul réglementaire (`NDCGenerator.cpp`).

---

## 4. Conventions de Commentaires Normatifs dans le Code

Pour maintenir la lisibilité sans surcharge artificielle, les marqueurs normalisés suivants doivent être appliqués aux points névralgiques du code :

### Architecture et Découplage (ISO/IEC 25010)
```cpp
// ============================================================
// NORMATIVE REFERENCE
// Standard   : ISO/IEC 25010:2023 §4.2.7 (Maintainability - Modularity)
// Area       : Domain Model / Single Source of Truth
// Requirement: REQ-SW-ARCH-002 (Structural Model as Single Truth)
// Interface  : IModelObserver (Asserved Observers Pattern)
// Constraint : Model must remain agnostic of UI and OCCT shapes
// ============================================================
```

### Calcul Réglementaire Eurocode
```cpp
// ============================================================
// STRUCTURAL STANDARD
// Standard   : EN 1990:2002+A1:2005 (Eurocode 0 - Bases de calcul)
// Clause     : §6.4.3 (Expressions 6.10, 6.10a/b) & §6.5.3 (ELS)
// Requirement: REQ-CALC-EC0-001 (Load Combinations ULS / SLS)
// Test ID    : TSA_LoadsTests / TSA_OpenSeesTests
// ============================================================
```

### Données d'Ingénierie & Propriétés Physiques
```cpp
// ============================================================
// STRUCTURAL STANDARD & DATA DEFINITION
// Standard   : EN 1992-1-1:2004 Table 3.1 (Concrete) & EN 1993-1-1:2005 Table 3.1 (Steel)
// Requirement: REQ-DATA-SI-001 (Strict SI Units: Pa, kg/m3) & REQ-CALC-EC2-001
// Validator  : TSA::Standards::ModelValidator::validateMaterial
// Decoupling : Mechanical properties (E, nu, rho, fk) strictly decoupled from PBR visual properties
// ============================================================
```

### Interface de Bibliothèque Externe
```cpp
// ============================================================
// EXTERNAL LIBRARY
// Library    : OpenSees (Open System for Earthquake Engineering Simulation)
// Version    : 3.4.0 - 3.8.0+
// Role       : Structural Analysis Backend (FEM)
// Interface  : Adapter converting TSA StructuralModel / Snapshot to Tcl scripts
// Constraint : TSA must not expose OpenSees implementation details to UI/Model
// Standard   : ISO/IEC 25010 §4.2.7 / Requirement: REQ-EXT-LIB-001
// ============================================================
```

---

## 5. Matrice de Traçabilité des Exigences (État Actuel)

| ID | Domaine | Norme | Clause | Description de l'Exigence | Code Source | Test | Statut |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :---: |
| **REQ-SW-ARCH-001** | Architecture | ISO/IEC 25010 | §4.2.7 | Découplage strict des couches : UI $\to$ Command $\to$ Model $\to$ Geometry $\to$ OCCT | `MainWindow_Actions.cpp`, `OccView.cpp` | `TSA_CommandsUndoTests` | **IMPLEMENTED** |
| **REQ-SW-ARCH-002** | Architecture | ISO/IEC 25010 | §4.2.7 | Modèle structural comme unique source de vérité (`IModelObserver`) | `Model.h`, `OccView.h` | `TSA_ModelElementsTests` | **IMPLEMENTED** |
| **REQ-SW-MOD-001** | Architecture | ISO/IEC 25010 | §4.2.7 | Surveillance SRP des modules > 1000 lignes et découplage modulaire | `Model.cpp`, `TSAFile.cpp` | `TSA_AllTests` | **IMPLEMENTED** |
| **REQ-SW-TEST-001** | Tests | ISO/IEC/IEEE 29119 | Part. 2/3 | Stratégie de tests automatisés (16 suites CTest, non-régression) | `tests/` | `TSA_AllTests` | **IMPLEMENTED** |
| **REQ-SW-DOC-001** | Documentation | IEEE Std 1063 | Clause 5 | Documentation technique et raccourcis synchronisés sans écart | `CommandCatalog.cpp`, `shortcuts.txt` | `check_shortcuts.py` | **IMPLEMENTED** |
| **REQ-EXT-LIB-001** | Architecture | ISO/IEC 25010 | §4.2.7 | Inventaire et isolation des bibliothèques externes (Qt, OCCT, OpenSees) | `ExternalLibraryCatalog.cpp` | `TSA_StandardsTests` | **IMPLEMENTED** |
| **REQ-DATA-SI-001** | Données | ISO 80000-1 / EN 1990 | §1.6 | Unités SI strictes ($m, m^2, m^4, N, kN, Pa, MPa, kg/m^3$) | `Section.h`, `Material.h` | `TSA_ModelElementsTests` | **IMPLEMENTED** |
| **REQ-DATA-VAL-001** | Intégrité | ISO/IEC 25010 | §4.2.5 | Rejet défensif des coordonnées NaN/inf, sections négatives, instabilités | `ModelValidator.cpp` | `TSA_StandardsTests` | **IMPLEMENTED** |
| **REQ-CALC-SNAP-001** | Analyse EF | ISO/IEC 25010 | §4.2.5 | Snapshot immuable (`CalculationSnapshot`) isolant le calcul de l'UI | `CalculationSnapshot.h` | `TSA_OpenSeesTests` | **IMPLEMENTED** |
| **REQ-CALC-EQ-001** | Analyse EF | RDM | Statique | Équilibre global rigoureux ($\sum \vec{F}_{ext} + \sum \vec{R} \approx \vec{0}$) | `OpenSeesResultsReader.cpp` | `TSA_OpenSeesTests` | **IMPLEMENTED** |
| **REQ-CALC-EC0-001** | Eurocodes | EN 1990 | §6.4.3 | Combinaisons d'actions fondamentales ELU et caractéristiques ELS | `LoadCombination.h` | `TSA_LoadsTests` | **IMPLEMENTED** |
| **REQ-CALC-EC0-002** | Eurocodes | EN 1990 | Annexe A1 | Facteurs partiels $\gamma$ et $\psi$ configurables par Annexe Nationale | `NationalAnnexConfig.h` | `TSA_StandardsTests` | **IMPLEMENTED** |
| **REQ-CALC-EC1-001** | Eurocodes | EN 1991 | §5.2 | Poids propre volumique automatique ($\rho \cdot g \cdot A$) | `OpenSeesAnalysisBuilder.cpp` | `TSA_OpenSeesTests` | **IMPLEMENTED** |
| **REQ-CALC-EC2-001** | Eurocodes | EN 1992 | Tab. 3.1 | Bétons normés C20/25 à C50/60 ($f_{ck}, E_{cm}, \nu$) | `Material.cpp` | `TSA_ExtensionsTests` | **IMPLEMENTED** |
| **REQ-CALC-EC2-002** | Eurocodes | EN 1992 | §6.1 | Dimensionnement du ferraillage longitudinal en flexion | `src/Design/` | Prévu Phase 6 | **PARTIAL** |
| **REQ-CALC-EC3-001** | Eurocodes | EN 1993 | Tab. 3.1 | Aciers S235/S355 et profilés normalisés IPE/HEA/HEB/UPN | `Section.cpp`, `Material.cpp` | `TSA_ModelElementsTests` | **IMPLEMENTED** |
| **REQ-CALC-EC3-002** | Eurocodes | EN 1993 | §6.3 | Justification des barres aux instabilités (flambement, déversement) | `src/Design/` | Prévu Phase 6 | **PARTIAL** |
| **REQ-CALC-CAB-001** | Câbles | EN 1993-1-11 | §5 & §6 | Câbles tendus, module d'Ernst, haubans, rentrée de mors | `CableStandards.cpp` | `TSA_CablesTests` | **IMPLEMENTED** |
| **REQ-UI-PORT-001** | Ergonomie | ISO/IEC 25010 | §4.2.4 | Espace multi-ports réactif sans duplication mémoire du modèle | `PortAreaWidget.cpp` | `TSA_WindowManagerTests` | **IMPLEMENTED** |
| **REQ-NDC-GEN-001** | Rapport | IEEE Std 1063 | Justification | Note de calcul réglementaire certifiée HTML / Texte brut | `NDCGenerator.cpp` | `TSA_AllTests` | **IMPLEMENTED** |
| **REQ-RES-META-001** | Résultats | ISO/IEC 25010 | §4.2.5 | Métadonnées d'exécution certifiées (Annexe Nationale, OpenSees, résidu max) | `ResultsModel.h`, `OpenSeesSolver.cpp` | `TSA_StandardsTests` | **IMPLEMENTED** |
| **REQ-VV-BENCH-001** | Vérification & Validation | ISO/IEC/IEEE 29119 | Part. 2/3 | Benchmarks analytiques fermés (Euler-Bernoulli, Timoshenko, Navier) tol. <= 2% | `AnalyticalBenchmark.cpp` | `TSA_StandardsTests` | **IMPLEMENTED** |
| **REQ-VIS-OCCT-001** | Visualisation 3D | OCCT 8.0 / C++20 | API Moderne | Conformité OCCT 8.0 sans dépréciation (`false` standard au lieu de `Standard_False`) | `ResultsVisualManager.cpp`, `CableGeometry3D.cpp` | `TSA_ViewerTests` | **IMPLEMENTED** |
| **REQ-VIS-NUM-001** | Visualisation 3D | ISO/IEC 25010 | §4.2.5 | Robustesse géométrique défensive (gardes NaN/Inf, échelles négatives, déformées et diagrammes) | `DiagramGeometry.cpp`, `DeformedGeometry.cpp` | `TSA_OpenSeesTests` | **IMPLEMENTED** |
