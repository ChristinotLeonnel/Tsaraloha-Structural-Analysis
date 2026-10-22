# TSA — Directives Fondamentales pour Agents IA & Développeurs

> **Document Maître de Référence (Niveau 1 — Règles Générales)**
> Ce fichier définit les principes directeurs, l'architecture fondamentale et les règles de développement de **TSA (Tsaraloha Structural Analysis)**.
> Les règles thématiques détaillées sont dans [`.agents/rules/`](file:///d:/Tsaraloha%20Platforme/TSA/.agents/rules).
> Les procédures opérationnelles courantes sont dans [`.agents/skills/`](file:///d:/Tsaraloha%20Platforme/TSA/.agents/skills).
> La documentation technique vivante est dans [`docs/`](file:///d:/Tsaraloha%20Platforme/TSA/docs).

---

## 1. Mission & Stack Réelle du Projet

TSA est une suite logicielle desktop de modélisation CAO 3D B-Rep exacte et d'analyse structurelle par éléments finis (FEA) pour le génie civil (poutres, poteaux, dalles, voiles, fondations, treillis, câbles), avec exploitation graphique interactive et génération de notes de calculs selon les **Eurocodes (EN 1990 à EN 1999)**.

Le code source dans `src/` constitue une **base technique commune partagée** avec le logiciel frère *TSALab* (ADR-023 / ADR-024) :
- **Langage & Standard** : C++20 (`CMAKE_CXX_STANDARD 20`), compilé sous MSVC 19.51 (Visual Studio 2026/2022 x64) ou MinGW-w64.
- **Interface Utilisateur** : Qt 6.2+ (Widgets, Gui, Core, Svg, Network).
- **Noyau Géométrique CAO 3D** : OpenCASCADE Technology (OCCT) 8.0.1 (B-Rep volumique exact, AIS, V3d).
- **Moteur Éléments Finis Principal** : OpenSees 3.8.0 (processus externe piloté par `QProcess` via scripts Tcl).
- **Maillage Éléments Finis** : Gmsh 4.15.2 SDK (encapsulé dans `src/Mesh/Gmsh/`).
- **Interopérabilité BIM** : IfcOpenShell 0.9.0 (import/export IFC STEP ISO 10303-21).
- **Cœur Scientifique Partagé** : `tsalab_science` (dépôt frère `../TSALab/science`, solveur 2D analytique `MetDeDeplacement`).
- **Système de Build** : CMake (>= 3.20) avec presets **Ninja** (`ninja-debug`, `ninja-release`).
- **Extensions Dynamiques** : Système **TSALib** (profilés et matériaux chargés à chaud sans recompilation).

---

## 2. Principes Fondamentaux Intangibles (Les 5 Piliers)

### 1. Modèle Structural = Source de Vérité Unique
Le modèle structural ([`TSA::Model::Model`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Model.h) et ses entités `Node`, `Beam`, `Column`, `Slab`, `Wall`, `Foundation`, `TrussMember`, `Cable`, `Section`, `Material`, `LoadManager`) est l'unique détenteur de l'état métier.
- **Interdiction formelle** de stocker l'état métier dans un widget UI, une variable d'affichage ou une forme graphique OCCT (`AIS_Shape`, `TopoDS_Shape`).
- Toute représentation graphique ou vue est strictement dérivée du modèle.

### 2. Flux Architectural Unidirectionnel
Les dépendances entre couches respectent un flux strict sans court-circuit :
```text
UI (src/UI : Ribbon, Docks, Properties, ModelTree)
 ↓ émet
Commandes / Transactions (src/Commands : ICommand, src/UndoRedo : EditTransaction)
 ↓ applique à
Modèle Structural (src/Model : Model — Source de vérité unique)
 ↓ notifie via IModelObserver
Constructeurs Géométriques (src/Geometry : *Geometry builders)
 ↓ génère
Représentation 3D OCCT (src/Viewer : OccView, SelectionManager)
```
Pour le calcul de structures :
```text
Modèle Structural → Snapshot Immuable (CalculationSnapshot) → Analyse Multi-Moteurs → Solveur (OpenSees / Custom2D) → ResultsModel → Affichage & NDC
```
- ❌ Pas de `UI → OCCT` direct sans passer par le modèle.
- ❌ Pas de `UI → Solveur` direct.
- ❌ Pas de `UI → Fichier (.tsa)` direct (passer par `ProjectManager` / `IO`).
- ❌ Pas de mutation directe du modèle depuis l'UI sans commande ou transaction.

### 3. Réutilisation Avant Création
Avant de créer une nouvelle classe, interface, commande, widget ou bibliothèque :
1. Rechercher l'existant dans `src/` ;
2. Comprendre son fonctionnement (lire l'implémentation `.cpp`, pas seulement le `.h`) ;
3. Vérifier s'il peut être étendu ou paramétré ;
4. Créer un nouvel élément uniquement si aucune réutilisation propre n'est possible.
*Ne jamais introduire d'implémentation parallèle concurrente d'une infrastructure existante (un seul UndoManager, un seul SelectionManager, un seul CommandCatalog).*

### 4. Synchronisation Réactive via `IModelObserver`
La cohérence entre modèle et affichage est assurée par le patron observateur granulaire [`TSA::Model::IModelObserver`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Model.h) (`on<Element>Added`, `on<Element>Modified`, `on<Element>Removed`) :
- `OccView` écoute ces callbacks pour régénérer **uniquement** la géométrie de l'élément modifié (pas de reconstruction globale intempestive de la scène 3D).
- `ResultsValidityGuard` invalide automatiquement les résultats périmés dès qu'une entité du modèle est modifiée.
- La sélection dans le viewport 3D renvoie à l'ID de l'élément du modèle, qui pilote contextuellement le panneau de propriétés ([`PropertyPanel`](file:///d:/Tsaraloha%20Platforme/TSA/src/UI/Properties/PropertyPanel.h)).

### 5. Rigueur Scientifique, Unités & Conventions Normatives
- **Unités Système International (SI)** : Longueurs en mètres ($m$), sections en millimètres ($mm$), forces en kilonewtons ($kN$), moments en kilonewtons-mètres ($kNm$), contraintes en mégapascals ($MPa$ ou $N/mm^2$).
- **Système de Coordonnées** : Repère global orthonormé direct avec l'axe $+Z$ vertical ascendant vers le haut.
- **Conformité aux Eurocodes** : Combinaisons de charges ELU / ELS selon l'EN 1990, dimensionnement selon l'EN 1992 (EC2) et l'EN 1993 (EC3).

---

## 3. Workflow de Développement Proportionné au Risque

Fini les procédures lourdes et bloquantes indifférenciées. L'ampleur des analyses et des tests doit être strictement proportionnée au risque réel de la modification :

```text
COMPRENDRE LE BESOIN
        ↓
INSPECTION CIBLÉE (sous-système concerné)
        ↓
IMPLÉMENTATION
        ↓
VALIDATION PROPORTIONNÉE AU RISQUE
        ↓
VÉRIFICATION DES EFFETS DE BORD
```

### Matrice de Validation Proportionnée

| Nature de l'Intervention | Périmètre des Contrôles | Validation Requise |
| :--- | :--- | :--- |
| **Correction mineure / UI locale** | Fichier modifié et appelants immédiats | Compilation sans erreur + test ciblé de la suite (`TSA_TestSuite --suite=<nom>`) |
| **Ajout/Modification de Commande** | `src/Commands`, UI appelante, raccourcis | Inscription dans `CommandCatalog.cpp`, test `--suite=commands` |
| **Évolution du Modèle ou Persistance** | `src/Model`, `src/IO`, snapshots | Tests `--suite=model --suite=io`, contrôle de non-régression du format `.tsa` |
| **Moteurs de Calcul / Éléments Finis** | `src/Analysis`, solveurs, mapping | Validation sur cas tests de référence (`TSA_TestSuite --suite=opensees` ou `TSA_Benchmark`) |
| **Changement Architectural Majeur** | Multiples sous-systèmes transversaux | Plan préalable documenté, exécution complète de `TSA_TestSuite` + `check_layers.py` |

> [!IMPORTANT]
> **Règle d'efficacité** : Ne pas lancer un audit complet du dépôt lorsqu'une tâche demande une correction ou une fonctionnalité ciblée. Inspecter uniquement les fichiers et sous-systèmes directement liés.

---

## 4. Règles de Conception et Qualité du Code

### Modularité & Responsabilité Unique (SRP)
Chaque classe et fonction doit avoir une responsabilité principale claire et identifiable.
- Éviter d'encombrer les classes UI (`MainWindow`) avec du calcul FEM, de la géométrie OCCT ou des entrées/sorties directes.
- Séparer nettement les données métier ([`src/Model`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model)), la géométrie B-Rep solide ([`src/Geometry`](file:///d:/Tsaraloha%20Platforme/TSA/src/Geometry)) et l'affichage interactif ([`src/Viewer`](file:///d:/Tsaraloha%20Platforme/TSA/src/Viewer)).

### Surveillance de la Taille du Code (Indicateurs Souples, Non Dogmatiques)
La taille d'un fichier s'évalue en fonction de sa responsabilité, sa complexité et sa lisibilité :
- `< 300 lignes` : Très confortable.
- `300–600 lignes` : Taille normale pour un composant C++.
- `600–1 000 lignes` : Surveiller la cohésion des responsabilités.
- `> 1 000 lignes` : Signal d'analyse pour évaluer si une séparation modulaire naturelle existe.
*Ne jamais découper artificiellement un fichier ni compresser du code pour satisfaire une métrique numérique.*

### Règle pour les Dépendances Externes
Avant d'introduire une nouvelle bibliothèque tierce dans le projet :
1. Vérifier si Qt, OpenCASCADE, Eigen, Boost ou les composants déjà liés couvrent le besoin.
2. Critères stricts d'acceptation : C++20, compatibilité Windows x64 (MSVC et MinGW), intégration CMake propre, licence permissive compatible (MIT, Apache 2.0, BSD, LGPL).
3. **Interdiction formelle d'inventer des APIs, des numéros de version ou des bibliothèques imaginaires.**

---

## 5. Gestion des Commandes & Undo/Redo

1. **Pattern Command** : Toute action de l'utilisateur qui mute le modèle structural doit être encapsulée dans une commande dérivant de [`TSA::Commands::ICommand`](file:///d:/Tsaraloha%20Platforme/TSA/src/Commands/ICommand.h) ou une transaction [`TSA::UndoRedo::EditTransaction`](file:///d:/Tsaraloha%20Platforme/TSA/src/UndoRedo/EditTransaction.h).
2. **Catalogue Centralisé** : Chaque commande est enregistrée dans [`src/Commands/CommandCatalog.cpp`](file:///d:/Tsaraloha%20Platforme/TSA/src/Commands/CommandCatalog.cpp) avec son identifiant canonique (`cmd.<categorie>.<action>`), son raccourci clavier et ses libellés.
3. **Undo/Redo via Snapshots** : [`UndoManager`](file:///d:/Tsaraloha%20Platforme/TSA/src/UndoRedo/UndoManager.h) restaure l'état via des clichés d'états immuables (`ModelStateSnapshot`). La synchronisation graphique post-undo utilise [`ModelDiff`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/ModelDiff.h) pour mettre à jour la scène sans reconstruction complète.
4. **Pas de Doublons de Commandes** : Réutiliser les commandes génériques (`CreateElementCommand`, `ModifyCommands`) au lieu de créer une commande isolée pour chaque variante d'élément, sauf workflow fondamentalement distinct.

---

## 6. Git, Build & Validation

### Protection du Travail Local & Règles Git
- **Préservation absolue** : Ne jamais exécuter de `git reset --hard` ni de `git clean -fd` sans accord explicite. Ne jamais écraser de travail local.
- **Branches dédiées** : Travailler sur des branches thématiques (`feature/<nom>`, `fix/<nom>`, `refactor/<nom>`, `ui/<nom>`, `docs/<nom>`). Ne pas pousser directement sur `main`.
- **Format des commits** : Format conventionnel `type: message clair et concis` (`feat:`, `fix:`, `refactor:`, `perf:`, `ui:`, `docs:`, `test:`).

### Compilation Rapide avec Ninja (MSVC)
```powershell
# 1. Charger l'environnement MSVC (une seule fois par session)
cmd /c "`"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && cmake --preset ninja-debug && cmake --build --preset ninja-debug -- -k 0 -j 4"

# 2. Exécuter la suite de tests (totale ou ciblée)
.\build-ninja-debug\TSA_TestSuite.exe                # Toutes les suites
.\build-ninja-debug\TSA_TestSuite.exe --suite=model  # Suite ciblée
```
*(Remarque : `-j 4` prévient les dépassements de mémoire virtuelle PCH sur les configurations à mémoire contrainte).*

### Règle d'Or des Tests de Non-Régression
- Tout test précédemment PASS doit rester PASS.
- **Interdiction formelle de désactiver, commenter ou affaiblir un test existant pour masquer une régression.** Corriger l'implémentation.
- Tout nouveau composant ou fonctionnalité métier doit être accompagné de son test unitaire dans [`tests/`](file:///d:/Tsaraloha%20Platforme/TSA/tests).

---

## 7. Index des Règles Spécialisées (Niveau 2)

Pour les exigences détaillées par sous-système, consulter les règles modulaires dédiées :
- [Architecture & Couplage](file:///d:/Tsaraloha%20Platforme/TSA/.agents/rules/01-architecture.md) — Découpage en couches, responsabilités et relations autorisées.
- [C++ & CMake](file:///d:/Tsaraloha%20Platforme/TSA/.agents/rules/02-cpp-cmake.md) — Normes C++20, RAII, gestion PCH et configuration multi-toolchain.
- [Qt & Interface Utilisateur](file:///d:/Tsaraloha%20Platforme/TSA/.agents/rules/03-qt-ui.md) — Ergonomie, Ribbon, docks, PropertyPanel et widgets.
- [OpenCASCADE & 3D](file:///d:/Tsaraloha%20Platforme/TSA/.agents/rules/04-occt-3d.md) — Gestion d'OccView, builders géométriques B-Rep et gizmo.
- [Modèle Structural](file:///d:/Tsaraloha%20Platforme/TSA/.agents/rules/05-structural-model.md) — Hiérarchie `Element`, nœuds, profilés, matériaux et pureté métier.
- [Synchronisation](file:///d:/Tsaraloha%20Platforme/TSA/.agents/rules/06-synchronization.md) — Contrat `IModelObserver`, réactivité et cohérence bidirectionnelle.
- [Git & Stratégie de Test](file:///d:/Tsaraloha%20Platforme/TSA/.agents/rules/07-git-testing.md) — Conventions de branches, exécution des suites de test.
- [Documentation & Normes](file:///d:/Tsaraloha%20Platforme/TSA/.agents/rules/08-documentation-and-standards.md) — Norme IEEE Std 1063 pour la documentation utilisateur et Eurocodes.