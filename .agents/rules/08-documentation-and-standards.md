---
title: Documentation & Normes Internationales (IEEE Std 1063, ISO/IEC/IEEE 26514, ISO/IEC 25010)
scope: repo
applies_to: ["**"]
---

# 08 — Documentation & Normes Internationales

Ce document définit les exigences obligatoires pour la rédaction de toute documentation (utilisateur et technique) ainsi que le cadre normatif international à respecter lors de toute modification future du logiciel TSA (Tsaraloha Structural Analysis).

---

## 1. Cadre Normatif Applicable

Toute évolution, refactorisation, correction ou documentation dans le dépôt TSA doit s'inscrire dans le respect des normes internationales suivantes :

| Norme | Domaine | Rôle dans le projet TSA |
| :--- | :--- | :--- |
| **IEEE Std 1063-2001 (R2007)** | *Software User Documentation* | Structure, contenu et qualité minimale de la documentation utilisateur (manuels, raccourcis, aides intégrées). |
| **ISO/IEC/IEEE 26514:2022** | *Requirements for designers & developers of user documentation* | Processus de conception, cohérence terminologique et ergonomie documentaire. |
| **ISO/IEC/IEEE 12207:2017** | *Systems and software engineering — Software life cycle processes* | Cycle de développement ordonné (Analyse → Conception → Implémentation → Validation). |
| **ISO/IEC/IEEE 29148:2018** | *Requirements engineering* | Spécification d'exigences vérifiables, atomiques et sans ambiguïté. |
| **ISO/IEC/IEEE 29119** | *Software testing* | Stratégie de tests automatisés, tests unitaires et de non-régression. |
| **ISO/IEC 25010:2023** | *Systems and software Quality Requirements and Evaluation (SQuaRE)* | Critères qualité du produit logiciel (fiabilité, maintenabilité, performance, adéquation fonctionnelle). |
| **Eurocodes (EN 1990 à EN 1999)** | *Conception des structures (Génie Civil)* | Normes métier de référence : unités SI strictes, repères orthonormés, conventions des profilés et des matériaux. |

---

## 2. Norme IEEE Std 1063 — Application à la Documentation

Tout document destiné aux utilisateurs (guides, aide en ligne, liste des raccourcis, référence des commandes console) doit obligatoirement intégrer la structure et les exigences de l'**IEEE Std 1063** :

### 2.1 Structure Minimale Obligatoire d'un Document Utilisateur

Conformément à l'IEEE Std 1063 §5, tout document utilisateur formel doit comporter :

1. **Identification du document (Clause 5.2) :**
   - Titre précis et sous-titre explicite.
   - Identifiant unique du document et version du document.
   - Version logicielle TSA cible (ex. `TSA v2026.09`).
   - Date de publication ou de dernière révision.
   - Entité émettrice / copyright et statut du document.
   - Journal des modifications (Revision History).

2. **Portée, Public Cible et Prérequis (Clause 5.3 & 5.4) :**
   - **Portée (Scope)** : objectifs couverts par le document et limites d'application.
   - **Public cible (Audience Profile)** : profil des lecteurs (ingénieurs structure, dessinateurs-projeteurs, développeurs, étudiants).
   - **Prérequis (Prerequisites)** : connaissances requises et configuration matérielle/logicielle requise.

3. **Conventions Typographiques et Notations (Clause 5.5) :**
   - Typographie (gras pour les actions UI, code pour les raccourcis et commandes).
   - Conventions pour les touches clavier (`Ctrl+<Touche>`, `Shift+<Touche>`, `Alt+<Touche>`, `Num+<Chiffre>`).
   - Conventions d'interaction 3D souris (Clic gauche, Clic droit, Roulette, Clic-glisser).
   - Unités de mesure (Système International strict : m, mm, kN, kNm, MPa).

4. **Contenu Fonctionnel et Référence (Clause 5.6 & 5.7) :**
   - Organisation hiérarchique logique par domaine d'activité.
   - Description pour chaque commande : nom complet, identifiant technique (`cmd.*`), raccourci clavier standard et alternatifs, commande console équivalente, préconditions, action produite, résultat attendu et erreurs éventuelles.

5. **Aides à la Navigation et Indexation (Clause 5.8) :**
   - Table des matières ou sommaire thématique.
   - Références croisées vers le code source C++ correspondant.

### 2.2 Critères de Qualité Documentaire (IEEE Std 1063 §4.2)

- **Exactitude (Accuracy)** : Chaque raccourci, commande ou procédure documentée doit être vérifiée sur le binaire réel. Zéro écart toléré entre la documentation et le comportement du logiciel.
- **Complétude (Completeness)** : Toute commande exposée dans l'interface ou le catalogue doit être documentée. Aucune commande "fantôme" ou non documentée.
- **Clarté (Clarity)** : Rédaction sans ambiguïté, définissant systématiquement les acronymes métier (LCS, OSNAP, PBR, IPE, HEA, etc.).
- **Cohérence (Consistency)** : Uniformité stricte de la terminologie entre le code (`CommandCatalog.cpp`), l'UI (`MainWindow_Actions.cpp`), les infobulles et les documents (`shortcuts.txt`, fichiers Markdown dans `docs/`).

---

## 3. Normes Internationales pour les Modifications Futures

Pour tout développement futur sur le dépôt TSA, les règles suivantes sont d'application obligatoire :

### 3.1 Méthode de Travail Ordonnée (ISO/IEC/IEEE 12207)

Ne jamais sauter immédiatement à l'implémentation de code sur une tâche non triviale :
```text
ANALYZE (Exigences ISO 29148)
   ↓
IDENTIFY RESPONSIBILITY (SRP - ISO 25010)
   ↓
CHECK EXISTING ARCHITECTURE (docs/ & src/)
   ↓
PLAN & TRACEABILITY
   ↓
IMPLEMENT
   ↓
BUILD (CMake / Ninja)
   ↓
TEST (Tests Unitaires ISO 29119)
   ↓
VERIFY (Documentation IEEE 1063)
```

### 3.2 Spécification et Traçabilité (ISO/IEC/IEEE 29148)

- Toute nouvelle action UI doit posséder son pendant dans le catalogue de commandes (`src/Commands/CommandCatalog.cpp`) avec un identifiant canonique `cmd.<catégorie>.<action>`.
- Chaque commande doit être documentée simultanément dans les fichiers de référence (`docs/shortcuts.txt` et `docs/`).
- L'outil de vérification `tools/check_shortcuts.py --strict` doit être exécuté et renvoyer impérativement `OK : raccourcis coherents.` avant toute validation.

### 3.3 Préservation de la Qualité Logicielle (ISO/IEC 25010)

- **Source de Vérité Unique** : Le modèle structural (`TSA::Model::Model`) est l'unique détenteur de l'état métier. L'UI et OpenCASCADE sont des consommateurs asservis via `IModelObserver`.
- **Faible Couplage** : Pas de dépendance croisée UI ↔ Solveur ou UI ↔ OpenCASCADE direct.
- **Gestion Défensive** : Vérification des pointeurs nuls, gestion des exceptions et non-régression géométrique.
- **Performance** : Invalidation ciblée du viewer 3D (pas de `fitAll()` automatique intempestif ou de reconstruction globale de scène).

### 3.4 Normes Métier de Génie Civil (Eurocodes)

- **Système de Coordonnées** : Repère global orthonormé direct avec l'axe Z vertical ascendant ($+Z$ vers le haut).
- **Unités Système International (SI)** : Longueurs en mètres ($m$), sections en millimètres ($mm$), forces en kilonewtons ($kN$), moments en kilonewtons-mètres ($kNm$), contraintes en mégapascals ($MPa$ ou $N/mm^2$).
- **Conventions d'accrochage (OSNAP)** : Standards CAO internationaux (Extrémité, Milieu, Centre, Intersection, Perpendiculaire, Nœud structural).

---

## 4. Outils de Contrôle Automatisé

Avant toute validation de modification :
1. **Compilation Ninja sans avertissement bloquant** :
   ```powershell
   cmake --build --preset ninja-debug -- -k 0
   ```
2. **Exécution complète de la suite de tests (ISO 29119)** :
   ```powershell
   .\build-ninja-debug\TSA_TestSuite.exe
   ```
3. **Vérification de cohérence documentaire (IEEE 1063)** :
   ```powershell
   python tools/check_shortcuts.py --strict
   ```
