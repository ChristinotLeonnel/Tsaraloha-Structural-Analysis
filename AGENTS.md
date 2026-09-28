# TSA — Directives Fondamentales pour Agents IA (Claude, Gemini, autres)

> Ce fichier est la référence générale pour tout agent IA (ou humain) travaillant sur TSA.
> Les règles détaillées et thématiques sont dans `.agents/rules/`.
> Les procédures pas-à-pas pour les tâches courantes sont dans `.agents/skills/`.
> Les rôles spécialisés (revue d'architecture, correction de bug) sont dans `.agents/agents/`.
>
> ⚠️ La section « Directives de Recherche Préalable » plus bas dans ce fichier existait avant
> la mise en place de ce framework et reste pleinement en vigueur : elle est complémentaire,
> pas remplacée.

## Mission

TSA (Tsaraloha Structural Analysis) est un logiciel desktop de modélisation et d'analyse des
structures de génie civil (poutres, poteaux, dalles, voiles, fondations, treillis, câbles),
avec visualisation 3D et export de calculs.

Stack réelle du dépôt :

- C++ (C++20)
- Qt 6 (Widgets)
- OpenCASCADE Technology (OCCT) — modélisation géométrique + viewer 3D (AIS/V3d)
- CMake (+ CMakePresets.json) — presets **Ninja** (`ninja-debug`, `ninja-release`) recommandés pour le
  développement ; presets Visual Studio (`windows-x64-*`) conservés (voir « Build et compilation »)
- Visual Studio / MSVC (toolchain principale), MinGW-w64 (alternative)
- Système d'extensions dynamique **TSALib** (bibliothèques de sections/matériaux chargées à chaud)

## Méthode obligatoire pour tout agent (Gemini, Claude, autres)

À chaque nouvelle tâche ou modification significative :

```text
ANALYZE
   ↓
IDENTIFY RESPONSIBILITY
   ↓
CHECK EXISTING ARCHITECTURE
   ↓
PLAN
   ↓
IMPLEMENT
   ↓
BUILD
   ↓
TEST
   ↓
VERIFY
```

Ne jamais sauter directement à `IMPLEMENT` sur une tâche non triviale. Utiliser le skill
`analyze-project` en phase `ANALYZE` si le contexte n'est pas déjà clair.

Avant d'ajouter du code :
```text
SEARCH EXISTING CODE
```
Avant de créer une classe :
```text
CHECK WHETHER AN EQUIVALENT CLASS ALREADY EXISTS
```
Avant de créer un système :
```text
CHECK WHETHER AN EXISTING SYSTEM CAN BE EXTENDED
```

## Règle fondamentale — Réutiliser avant de créer

Avant de créer une nouvelle classe, interface, bibliothèque ou abstraction :

1. rechercher l'existant dans `src/` ;
2. comprendre son fonctionnement (lire l'implémentation, pas seulement le header) ;
3. vérifier s'il peut être réutilisé ou étendu ;
4. seulement ensuite créer quelque chose de nouveau.

Cette règle rejoint et renforce la « Recherche Préalable Obligatoire sur Internet » définie
plus bas dans ce fichier : ici il s'agit de réutilisation **interne** (code déjà présent dans
TSA), là-bas de réutilisation **externe** (bibliothèques tierces).

## Règle de modularité et taille du code

Il n'existe pas de limite absolue de lignes de code pour un fichier, une classe ou une fonction.

La taille du code doit être évaluée en fonction de la **responsabilité**, de la **complexité**,
du **couplage**, de la **testabilité** et de la **lisibilité**.

Une classe ou une fonction doit être découpée lorsque sa responsabilité devient difficile à
comprendre, tester, maintenir ou modifier.

Éviter les fichiers monolithiques.

Lorsqu'un fichier dépasse environ **1 000 lignes**, effectuer systématiquement une analyse pour
déterminer si une séparation cohérente est possible.

Cette limite de 1 000 lignes est un **signal d'analyse et non une limite absolue**.

Ne jamais réduire artificiellement le nombre de lignes simplement pour respecter une métrique.
Il vaut mieux conserver un fichier plus long mais cohérent que créer plusieurs petites classes
artificielles ou fortement couplées.

### Échelle de surveillance

```text
< 300 lignes
→ taille généralement confortable

300–600 lignes
→ normale

600–1 000 lignes
→ surveiller la responsabilité

> 1 000 lignes
→ analyser systématiquement la possibilité de découpage

> 2 000 lignes
→ refactorisation à envisager sérieusement

> 5 000 lignes
→ fichier potentiellement monolithique ; analyse architecturale obligatoire
```

**IMPORTANT :** Ces valeurs sont des **indicateurs**, pas des règles mécaniques. Ne jamais
découper un fichier uniquement parce qu'il dépasse une valeur numérique.

## Règle de responsabilité unique (Single Responsibility Principle)

Une classe doit avoir une **responsabilité principale clairement identifiable**.

Exemples TSA :
- `Beam` → représente une poutre (données métier)
- `Column` → représente un poteau (données métier)
- `Cable` → représente un câble (données métier)
- `Section` → représente une section transversale
- `BeamGeometry` → construit la géométrie 3D d'une poutre
- `SelectionManager` → gère la sélection dans le viewport
- `WorkPlaneManager` → gère les plans de travail multiples

Éviter une classe monolithique du type `MainWindow` qui contiendrait simultanément :
```text
UI + modèle structural + calcul + géométrie OCCT + sélection + sauvegarde + undo/redo + gestion des matériaux
```
Si ce type de concentration existe déjà dans le code hérité, l'analyser soigneusement avant de
la modifier ou d'y ajouter de nouvelles responsabilités.

## Règle pour les fonctions

Une fonction doit réaliser une **tâche clairement identifiable**.

Si une fonction contient plusieurs responsabilités indépendantes :
```text
UI + validation + modification du modèle + création OCCT + sauvegarde
```
chercher à séparer les responsabilités.

Mais ne pas créer automatiquement une multitude de petites fonctions sans valeur architecturale
(pas de morcellement excessif au détriment de la lisibilité).

## Règle de couplage et flux architectural

Lors d'un nouveau développement, éviter absolument les raccourcis de couplage :
- ❌ `UI → OCCT` directement
- ❌ `UI → fichier (.tsa)` directement
- ❌ `UI → moteur de calcul (solveur)` directement
- ❌ `UI → données internes du modèle` directement (contournant les commandes)

Préférer, lorsque l'architecture existante le permet :
```text
UI
 ↓
Command / Application (ICommand, CommandManager)
 ↓
Structural Model (TSA::Model::Model — Source de vérité)
 ↓
Geometry (*Geometry builders)
 ↓
OCCT (AIS_Shape, OccView)
```

Et pour le calcul :
```text
Structural Model
 ↓
Analysis Model
 ↓
Solver / OpenSees Adapter
 ↓
Results
```

Le modèle structural reste en toute circonstance la **source de vérité unique**.

## Règle de refactorisation sécurisée

Ne jamais faire un grand refactoring sans nécessité démontrée.

Avant de découper une classe ou un fichier volumineux :
1. identifier sa responsabilité ;
2. identifier ses dépendances ;
3. identifier les appels entrants ;
4. identifier les appels sortants ;
5. vérifier les signaux/slots Qt ;
6. vérifier les références OCCT ;
7. vérifier les tests unitaires existants ;
8. vérifier CMake (`CMakeLists.txt`) : appartenance des fichiers à `CORE_SOURCES` ou `SOURCES`,
   impact sur le PCH (voir « Build et compilation ») ;
9. déterminer le risque de régression.

Puis seulement proposer ou appliquer le découpage.

## Objectifs des futures modifications

Ne cherche jamais à minimiser le nombre de lignes de code comme objectif principal.

Le but est :
```text
Maintenabilité + Lisibilité + Modularité + Faible couplage + Testabilité + Cohérence architecturale
```
et non :
```text
Nombre de lignes minimal
```

Éviter notamment :
- le code artificiellement compressé ou obscurci ;
- les fonctions gigantesques à effets de bord multiples ;
- les classes fourre-tout ;
- les abstractions inutiles et couches d'indirection vides ;
- la duplication de code métier ou géométrique ;
- les classes créées uniquement pour faire baisser artificiellement une métrique de lignes ;
- les fichiers séparés sans responsabilité propre.

## Règle absolue de priorité

Ne jamais modifier le comportement fonctionnel de TSA uniquement pour respecter une métrique
de lignes de code.

La priorité stricte est toujours :
```text
CORRECTNESS
   ↓
ARCHITECTURE
   ↓
MAINTAINABILITY
   ↓
TESTABILITY
   ↓
PERFORMANCE
   ↓
Taille du code
```

## Architecture réelle du dépôt

Constatée par inspection de `src/` (voir `.agents/rules/01-architecture.md` et
`docs/ARCHITECTURE.md` pour le détail) :

```text
UI (src/UI : Ribbon, Dock, Properties, ModelTree, Dialogs, Widgets, Theme, Ruler)
  ↓ signals/slots, Commands
Commands / UndoRedo (src/Commands : ICommand, CreateBeamCommand, GridCommands
                      src/UndoRedo : CommandManager, UndoManager)
  ↓ agit sur
Structural Model (src/Model : Model, Element/LinearElement/SurfaceElement,
                   Beam, Column, Slab, Wall, Foundation, TrussMember, Cable/*,
                   Node, Section, Material, MaterialLibrary)
  ↓ construit
Geometry (src/Geometry : BeamGeometry, SlabGeometry, WallGeometry,
          FoundationGeometry, CableGeometry3D)
  ↓ affiché par
OCCT / Viewer (src/Viewer : OccView, SelectionManager, MaterialVisual, TextureManager)
```

En complément, transversaux à ces couches :

- `src/Coordinate` : `CoordinateSystem`, `Point3D`, `LevelManager`/`Level` (étages),
  `CylindricalCoordinates`.
- `src/Grid` : grilles 3D paramétriques et accrochage (snap).
- `src/ExtensionSystem` : chargement dynamique des bibliothèques **TSALib** (sections,
  matériaux) sans recompilation.
- `src/IO` : sérialisation du format fichier `.tsa`.
- `src/Diagnostics` : télémétrie / diagnostics internes.
- `src/Interaction` : gestion des interactions utilisateur dans le viewport 3D.
- `src/Project`, `src/App`, `src/main.cpp` : bootstrap de l'application.

Adapter systématiquement cette représentation si le code réel a évolué depuis la rédaction de
ce document (voir la règle de vérification ci-dessous).

## Modèle structural = source de vérité

Le modèle structural (`TSA::Model::Model` et les classes qu'il possède : `Beam`, `Column`,
`Slab`, `Wall`, `Foundation`, `TrussMember`, `Cable`, `Node`, `Section`, `Material`) est la
**source de vérité**.

Ne jamais utiliser comme source principale des propriétés métier :

- un widget UI (`src/UI/**`) ;
- une variable graphique ou d'affichage ;
- une `TopoDS_Shape` / `AIS_Shape` OCCT.

Ces couches doivent toujours se **dériver** du modèle, jamais l'inverse.

## Synchronisation

Les propriétés doivent rester cohérentes dans les deux sens :

```text
UI → Model → Geometry → 3D (AIS/OccView)
```

et

```text
Sélection 3D → Model → UI
```

`OccView` implémente `TSA::Model::IModelObserver` : c'est le mécanisme réel de
synchronisation modèle → 3D constaté dans le code (callbacks `onBeamAdded`,
`onBeamModified`, `onBeamRemoved`, etc., un triplet par type d'élément). Toute nouvelle
propriété ou tout nouvel élément doit s'intégrer à ce mécanisme d'observation plutôt que
d'en créer un parallèle. Voir `.agents/rules/06-synchronization.md`.

## Éléments structuraux

Hiérarchie réelle (`src/Model/Element.h`) :

```text
Element (interface : id, name, typeName, volume, weight)
├── LinearElement (interface : startNodeId, endNodeId, section, material, length)
│     → implémenté par Beam, Column, TrussMember, Cable (src/Model/Cable/*)
└── SurfaceElement (interface : nodeIds, thickness, material, area)
      → implémenté par Slab, Wall
```

`Foundation` existe également dans `src/Model` : vérifier dans le code si elle dérive de
`SurfaceElement`, de `LinearElement`, ou si c'est une classe à part avant de la traiter comme
l'un ou l'autre (`TODO: VERIFY IN SOURCE` dans `docs/MODEL.md`).

Ne jamais transformer artificiellement un type d'élément en un autre (un `Cable` ne doit pas
devenir un `Beam` pour simplifier une tâche).

## Sections

Système centralisé dans `src/Model/Section.h` / `Section.cpp` : `struct Section` avec un
`enum class SectionShape { Rectangular, Circular, IShape, Pipe, BoxHollow, UPN, Angle,
TSection }` et des usines statiques (`Section::rectangular`, `Section::circular`,
`Section::ipe`, `Section::hea`, `Section::heb`, `Section::upn`, `Section::angle`,
`Section::tSection`, `Section::boxHollow`, `Section::pipe`, `Section::defaultLibrary`).

Une section sélectionnée dans l'UI (`src/UI/Properties/PropertyPanel`) doit être identique à
celle du modèle et à celle utilisée par la géométrie OCCT (`src/Geometry/*Geometry`). Voir
`.agents/skills/add-section/SKILL.md` pour la procédure de vérification (notamment lors du
Copy/Paste).

## OCCT

OCCT (via `src/Viewer/OccView` et `src/Geometry/*`) représente visuellement le modèle
structural. Éviter les reconstructions globales inutiles de la scène.

Pour tout problème 3D, suivre le flux réel :

```text
Model → Paramètres (Section/Material/Node) → *Geometry (Geometry Builder) → OCCT (TopoDS_Shape/AIS_Shape) → OccView (Viewer)
```

Lorsqu'un élément est copié (`src/Model/StructuralClipboard`), sa géométrie doit être
recréée depuis son modèle structural copié — jamais réutilisée telle quelle depuis la Shape
OCCT source.

## Undo / Redo

Le mécanisme réel repose sur `TSA::UndoRedo::UndoManager` (piles de
`Model::ModelStateSnapshot`) et `TSA::UndoRedo::CommandManager` (exécution de
`TSA::Commands::ICommand`). Privilégier la restauration d'état du modèle
(`UndoManager::undo`/`redo`) plutôt que de redessiner toute la scène OCCT à la main.

## Git — Gestion Automatique des Branches et Workflow

Pour toute modification du projet TSA, déterminer d'abord la nature du travail avant de modifier le code.
**Principe fondamental : NE PAS effectuer automatiquement toutes les modifications dans la branche courante.**

### 1. Conventions de Branches Dédiées

| Préfixe | Usage | Exemples |
| :--- | :--- | :--- |
| `feature/<nom-court>` | Nouvelle fonctionnalité | `feature/workplane-3d`, `feature/cable-element`, `feature/result-diagrams` |
| `fix/<nom-court>` | Correction de bug existant | `fix/workplane-selection`, `fix/circular-column-display`, `fix/grid-crash` |
| `refactor/<nom-court>` | Refactorisation sans ajout fonctionnel direct | `refactor/coordinate-system`, `refactor/occview-split` |
| `perf/<nom-court>` | Amélioration de performance | `perf/occt-scene-update`, `perf/spatial-indexing` |
| `ui/<nom-court>` | Modification visuelle ou ergonomique | `ui/workplane-marker`, `ui/toolbar-elements` |
| `docs/<nom-court>` | Modification uniquement documentaire / règles | `docs/git-branching-rules`, `docs/workplane-system` |
| `test/<nom-court>` | Ajout ou refonte dédiée aux tests | `test/modular-test-suites` |

### 2. Modification Commune / Transversale vs Modification Indépendante

- **Modification liée ou transversale indispensable :**
  Si la modification est petite, transversale, nécessaire à la tâche actuelle, directement liée à la branche courante, ou constitue une correction mineure indispensable à la fonctionnalité en cours → **rester dans la branche actuelle**.
  *Exemple :* Sur `feature/workplane-3d`, une correction de synchronisation mineure du WorkPlane reste sur `feature/workplane-3d`.
- **Modification indépendante :**
  Si une modification concerne une autre fonctionnalité, un bug extérieur ou une amélioration indépendante → **NE PAS** l'implémenter dans la branche courante. Créer une branche appropriée (ex. `fix/cable-section-display`).

### 3. Avant de Créer une Branche

Toujours exécuter et vérifier :
```bash
git status
git branch --show-current
```
Vérifier que le working tree est propre et mettre à jour la branche de base (`main` ou branche de référence) avant d'embrancher :
```bash
git switch main
git pull
git switch -c feature/<nom-court>
```

### 4. Préservation du Travail Existant

Ne jamais :
- supprimer les modifications locales de l'utilisateur ;
- faire un reset destructif (`git reset --hard` sans demande explicite) ;
- utiliser `git clean -fd` sans autorisation explicite ;
- écraser des commits existants ou forcer avec `git push --force` ;
- changer de branche avec des modifications non sauvegardées sans vérifier les conséquences.

### 5. Conventions de Commit

Format recommandé : `type: message clair et concis`
```text
feat: implement interactive 3D workplane
fix: adapt gizmo size to camera zoom
refactor: split OccView into modular translation units
ui: improve workplane CAD marker
test: add regression tests for cable tension
docs: update architecture documentation
```

### 6. Push et Fusion

- Pousser la branche correspondante vers le remote (`git push -u origin feature/<nom-court>`).
- Ne jamais pousser automatiquement vers `main`.
- Ne jamais effectuer de `push --force` destructif sauf demande explicite.

### 7. Identification Préalable Obligatoire

Avant toute modification importante, identifier explicitement :
- **TYPE :** `feature` / `fix` / `refactor` / `perf` / `ui` / `docs` / `test`
- **BRANCHE :** branche actuelle ou nouvelle branche
- **JUSTIFICATION :** pourquoi cette modification appartient à cette branche.

## Build et compilation (CMake / Ninja / PCH)

Section ajoutée après l'optimisation du temps de compilation. Le `CMakeLists.txt` et le
`CMakePresets.json` font foi : en cas de divergence, corriger ce fichier.

### Presets

| Preset | Générateur | Dossier de build | Usage |
| :--- | :--- | :--- | :--- |
| `ninja-debug` | Ninja + MSVC (`cl`) | `build-ninja-debug/` | Développement quotidien (rapide) |
| `ninja-release` | Ninja + MSVC (`cl`) | `build-ninja-release/` | Release rapide |
| `windows-x64-debug` | Visual Studio 18 2026 | `build-debug/` | Conservé tel quel |
| `windows-x64-release` | Visual Studio 18 2026 | `build/` | Conservé tel quel |

Les presets `ninja-*` héritent de `ninja-base` (caché) et utilisent `"strategy": "external"` pour
l'architecture et le toolset : **l'environnement MSVC doit déjà être chargé** dans le terminal.

### Lancer un build Ninja (PowerShell)

```powershell
# 1. Charger l'environnement Visual Studio (une fois par fenêtre)
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
& "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64

# 2. Vérifier les outils
where.exe cl
where.exe ninja

# 3. Configurer puis compiler (-k 0 : afficher toutes les erreurs d'un coup)
cmake --preset ninja-debug
cmake --build --preset ninja-debug -- -k 0
```

Symptôme « `CMAKE_MAKE_PROGRAM is not set` » = environnement MSVC / Ninja non chargé (pas un bug du
projet). `Set-ExecutionPolicy -Scope Process` n'agit que sur la fenêtre courante. Alternative sans
politique PowerShell : `cmd /k "…\VC\Auxiliary\Build\vcvars64.bat"`.

### Structure des cibles CMake

```text
TSA_Core   (bibliothèque OBJECT — CORE_SOURCES, compilée UNE SEULE FOIS)
   ├── TSA          (exécutable : SOURCES = UI + viewer + rendu + plateforme + ressources)
   └── TSA_Tests    (exécutable : tests/*.cpp, sortie TSA_TestSuite.exe)
```

Règles à respecter lors de l'ajout ou du déplacement de fichiers :

- Un fichier **[CORE]** (modèle, coordonnées, grilles logiques, commandes, IO, ExtensionSystem…) va
  dans un groupe inclus dans `CORE_SOURCES`. Il **ne doit jamais** être ajouté aussi à `SOURCES` ou
  `TEST_SOURCES` : il serait compilé deux fois (c'était le défaut d'origine).
- Un fichier **UI / viewer / rendu / plateforme** va dans `SOURCES` (exécutable uniquement).
- `TSA_Core` est partagé avec les tests : il ne doit dépendre d'aucun fichier UI ni de `Qt6::Svg`.
  Si une dépendance supplémentaire est réellement nécessaire au Core, l'ajouter à
  `target_link_libraries(TSA_Core PUBLIC …)` et vérifier que `TSA_Tests` compile toujours.
- `TSA_Core` est une bibliothèque `OBJECT` (et non `STATIC`) pour ne pas perdre d'initialisations
  statiques au lien.

### En-têtes précompilés (PCH)

- Le PCH est défini sur `TSA_Core` (Qt Core/Gui/Widgets, types de base OCCT `gp_*`/`TopoDS_Shape`,
  STL) et réutilisé par `TSA` et `TSA_Tests` (`REUSE_FROM`).
- Les trois cibles doivent conserver des **options de compilation identiques** : elles passent par
  `tsa_apply_common_settings()` (`/W4 /permissive- /utf-8`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, `/MP`
  avec les générateurs Visual Studio). Ne pas ajouter d'options ou de définitions propres à une seule
  cible sans vérifier que le PCH reste partageable.
- N'ajouter au PCH que des en-têtes **stables et tiers** (Qt, OCCT, STL). Jamais d'en-têtes du projet
  fréquemment modifiés : toute modification du PCH recompile tout.
- Les fichiers qui utilisent directement l'API Win32 (`MessageBox`, shell, dbghelp…) sont exclus du
  PCH via `SKIP_PRECOMPILE_HEADERS` (actuellement `src/Diagnostics/CrashHandler.cpp` et
  `src/Platform/WindowsAssociation.cpp`). Symptôme d'un fichier à ajouter à cette liste :
  `identificateur non déclaré` sur `MB_OK`, `MessageBoxA` ou une constante/fonction Windows alors que
  `<windows.h>` est inclus dans le fichier.

### Options CMake

| Option | Défaut | Rôle |
| :--- | :--- | :--- |
| `TSA_BUILD_TESTS` | `ON` | Compile `TSA_Tests` et enregistre les tests CTest |
| `TSA_USE_CCACHE` | `OFF` | Utilise `ccache` (ignoré avec les générateurs Visual Studio) |

`TSA_USE_CCACHE` est désactivé par défaut : avec MSVC, le support de `ccache` pour les en-têtes
précompilés est limité. À activer seulement après mesure.

### Points connus

- Les includes OCCT sont déclarés `SYSTEM` (compilation plus rapide, pas d'avertissements `/W4`
  provenant d'OCCT).
- Avertissements de dépréciation OCCT 8.0 encore présents dans le code du projet
  (`TColgp_HArray1OfPnt`, `TColgp_Array1OfPnt` → `NCollection_HArray1<gp_Pnt>` /
  `NCollection_Array1<gp_Pnt>` ; `Standard_False` → `false`), par ex. dans
  `src/Geometry/CableGeometry3D.cpp`. Sans effet sur la vitesse de build.
- L'étape `POST_BUILD` (`cmake/DeployDependencies.cmake`) copie les DLL OCCT/3rdparty et lance
  `windeployqt` **à chaque édition de liens de `TSA.exe`**. Piste d'amélioration possible (la rendre
  conditionnelle) — *information à confirmer par mesure avant toute modification*.

### Règles pour les agents

- Toute modification de `CMakeLists.txt` / `CMakePresets.json` suit les règles Git (branche
  `perf/…` ou `refactor/…`) et la règle « lecture seule avant approbation » (section 8).
- Après une modification CMake : reconfigurer (supprimer le dossier de build si nécessaire), compiler
  la cible complète avec `-- -k 0`, puis compiler et exécuter `TSA_Tests`.
- Ne pas ajouter un fichier dans deux listes de sources ; ne pas contourner `TSA_Core`.
- Avant de proposer une optimisation de build, **mesurer** (build complet et rebuild après
  modification d'un seul `.cpp`) et rapporter les temps ; ne pas affirmer un gain non mesuré.

## Validation

Après toute modification importante :

- compiler (`build-test` skill, cible `TSA_Tests` si pertinente) — de préférence avec le preset
  `ninja-debug` (voir « Build et compilation ») ;
- exécuter les tests (`tests/test_coordinates.cpp` et toute autre suite existante) ;
- vérifier les régressions ;
- vérifier la couche UI ;
- vérifier la couche modèle ;
- vérifier la couche OCCT/3D.

## Vérification du contenu de ce fichier

Ce document a été rédigé par inspection ponctuelle du dépôt à une date donnée. Le code réel
fait foi. Si une divergence apparaît entre ce fichier et le code, corriger ce fichier plutôt
que de se fier aveuglément à sa version actuelle.

---

# Directives de Recherche Préalable de Solutions & Dépendances Externes (existant, préservé)

_Le contenu ci-dessous existait déjà dans `AGENTS.md` avant la mise en place du framework
`.agents/`. Il est conservé intégralement car il reste en vigueur — il couvre notamment la
recherche de bibliothèques tierces, la traçabilité des décisions, l'interconnexion des
éléments du système, et l'organisation des commandes et des fenêtres._

# Directives de Développement TSA - Recherche Préalable de Solutions & Dépendances Externes

Pour toute nouvelle fonctionnalité, extension, bibliothèque, outil, composant visuel (icônes, widgets, thèmes) ou module dans le projet TSA, appliquer systématiquement la règle suivante :

---

## 1. Règle Principale : Recherche Préalable Obligatoire sur Internet

Avant de créer ou de coder nous-mêmes une extension, une bibliothèque ou un composant :
- **Toujours rechercher sur Internet s'il existe déjà une solution adaptée, maintenue, fiable et reconnue**.
- **Ne jamais créer ou réinventer une solution si une bibliothèque ou un composant existant répond proprement au besoin**.

---

## 2. Protocole de Recherche & Évaluation

Pour chaque besoin identifié :
1. **Identifier précisément le besoin technique ou visuel**.
2. **Rechercher les solutions existantes** (via `search_web`, documentation officielle, dépôts GitHub).
3. **Privilégier les sources officielles et reconnues** :
   - Dépôts GitHub officiels et maintenus
   - Documentation officielle Qt
   - Écosystème Microsoft / Windows SDK / MSVC
   - CMake packages & modules
   - MinGW-w64
   - OpenCASCADE (OCCT)
   - VTK
   - Projets open source établis et activement maintenus
4. **Vérifier les critères de compatibilité stricts avec TSA** :
   - Compatibilité Windows 10/11 x64
   - Compatibilité standard C++20
   - Compatibilité toolchains (MSVC / MinGW)
   - Compatibilité Qt 6
   - Intégration CMake native
   - Licence compatible (MIT, Apache 2.0, BSD, LGPL, etc.)
   - Maintenance active et date des dernières mises à jour
5. **Si une solution existante convient** : l'utiliser et l'intégrer au lieu de la recoder.
6. **Si plusieurs solutions existent** : les comparer techniquement avant sélection.
7. **Si aucune solution existante n'est adaptée** : créer une solution personnalisée.

---

## 3. Format d'Évaluation Obligatoire

Pour toute proposition ou étude de nouvelle fonctionnalité ou composant, présenter obligatoirement l'analyse sous ce format :

```text
Besoin :
...

Solution existante trouvée :
...

Source :
...

Compatibilité TSA :
...

Avantages :
...

Limites :
...

Solution personnalisée nécessaire :
Oui / Non
```

---

## 4. Conditions pour Développer une Solution Personnalisée

Une solution personnalisée n'est développée que si :
- Aucune solution existante ne répond au besoin technique.
- Les solutions existantes sont incompatibles avec l'ABI ou l'architecture de TSA.
- La licence est restrictive ou incompatible.
- Le projet tiers est abandonné ou obsolète.
- L'intégration de la dépendance est inutilement lourde/complexe comparée au besoin réel.
- **OU** une implémentation sur-mesure est démontrée comme étant nettement plus légère, plus rapide, plus stable, plus moderne, plus esthétique ou mieux intégrée à l'architecture TSA (comparaison comparative obligatoire préalable).

---

## 5. Application aux Composants Visuels et Graphiques

Cette règle s'applique identiquement aux :
- Systèmes d'icônes (Font Awesome, Material Symbols, Fluent UI, svg-icons)
- Widgets et composants d'interface Qt
- Panneaux, docks et rubans
- Thèmes et feuilles de style (QSS)
- Moteurs de calcul et algorithmes

---

## 6. Règle Obligatoire — Recherche, Sources et Références

Toute recherche technique effectuée pendant l'audit ou le développement de TSA doit être **traçable et accompagnée de références vérifiables**.

Cette règle s'applique notamment aux recherches concernant :

* architecture logicielle ;
* C++ ;
* Qt ;
* OCCT ;
* CMake ;
* Visual Studio / MSVC ;
* MinGW ;
* bibliothèques externes ;
* formats de fichiers ;
* géométrie ;
* éléments structuraux ;
* calcul de structures ;
* câbles ;
* matériaux ;
* sections ;
* Eurocodes ;
* méthodes numériques ;
* FEA/FEM ;
* interaction UI / modèle / OCCT ;
* Undo / Redo ;
* sauvegarde / chargement ;
* toute nouvelle technologie proposée pour TSA.

### 6.1 Sources à privilégier

Utiliser en priorité des sources fiables et directement pertinentes :

#### a) Documentation officielle

* Qt
* Open CASCADE Technology
* CMake
* Microsoft / Visual Studio
* MinGW
* bibliothèques utilisées par TSA

#### b) Normes et documents officiels

* Eurocodes
* normes ISO/EN lorsqu'elles sont pertinentes
* documents officiels des organismes concernés

#### c) Livres techniques reconnus

Pour un livre, indiquer autant que possible :

```text
Titre
Auteur(s)
Édition
Éditeur
Chapitre / section
```

#### d) Articles scientifiques et publications universitaires

Indiquer :

```text
Titre
Auteur(s)
Journal / conférence
Année
DOI ou référence disponible
```

#### e) Dépôts officiels

Par exemple :

* GitHub officiel d'un projet ;
* GitLab officiel ;
* documentation officielle du projet ;
* dépôt officiel contenant le code source.

### 6.2 Sources secondaires

Les forums, blogs, Stack Overflow, Reddit, vidéos et autres sources communautaires peuvent être utilisés pour rechercher des pistes ou comprendre un problème, mais ils ne doivent pas être considérés automatiquement comme une autorité technique.

Lorsqu'une information importante provient d'une source secondaire, rechercher si possible la documentation officielle ou la source primaire correspondante.

### 6.3 Aucune référence inventée

**INTERDICTION ABSOLUE d'inventer une référence.**

Ne jamais fabriquer :

* URL ;
* titre de documentation ;
* nom de livre ;
* auteur ;
* DOI ;
* numéro de norme ;
* chapitre ;
* numéro de section ;
* version de bibliothèque ;
* fonction ou API supposée.

Si une information ne peut pas être vérifiée, le préciser clairement :

```text
Source non vérifiée
```

ou :

```text
Information à confirmer
```

plutôt que d'inventer une source.

### 6.4 Chaque décision technique importante doit être justifiée

Lorsqu'une recherche conduit à une décision d'architecture ou d'implémentation, utiliser autant que possible le format :

```text
Décision :
...

Pourquoi :
...

Source :
...

Référence :
...

Impact sur TSA :
...
```

Exemple :

```text
Décision :
Réutiliser le système de commandes existant pour les modifications
de propriétés du Cable.

Pourquoi :
Le système de commandes actuel fournit déjà Undo/Redo et permet
d'éviter la création d'un deuxième système indépendant.

Source :
Analyse du code existant TSA.

Référence :
Fichier / classe / fonction concernée.

Impact sur TSA :
Le Cable utilise la même infrastructure de commandes que les autres
éléments tout en conservant ses propres propriétés.
```

Lorsqu'une décision repose sur une documentation externe :

```text
Décision
    ↓
Justification technique
    ↓
Source primaire
    ↓
Référence vérifiable
```

### 6.5 Comparaison des solutions

Lorsqu'il existe plusieurs solutions possibles, ne pas choisir immédiatement.

Comparer d'abord les solutions pertinentes.

Exemple :

```text
Solution A
Source :
Compatibilité :
Avantages :
Limites :

Solution B
Source :
Compatibilité :
Avantages :
Limites :

Solution retenue :
...

Justification :
...
```

La solution retenue doit être justifiée par des critères techniques liés au projet TSA et non par une préférence arbitraire.

### 6.6 Recherche sur les bibliothèques externes

Avant d'ajouter une nouvelle dépendance :

```text
Besoin
    ↓
Recherche de solutions existantes
    ↓
Documentation officielle
    ↓
Licence
    ↓
Maintenance
    ↓
Compatibilité TSA
    ↓
Décision
```

Vérifier notamment :

```text
C++
Qt 6
CMake
Windows
MSVC
MinGW
OCCT
Architecture actuelle de TSA
Licence
Maintenance du projet
```

Ne pas ajouter une bibliothèque uniquement parce qu'elle semble résoudre rapidement un problème.

### 6.7 Recherche sur les éléments structuraux

Pour toute fonctionnalité liée au calcul ou à la modélisation structurale, rechercher autant que possible les références techniques correspondantes.

Exemples :

```text
Cable
    ↓
Mécanique des câbles
    ↓
Référence technique / livre / norme / publication
```

```text
Section acier
    ↓
Géométrie du profil
    ↓
Norme / catalogue / documentation
```

```text
Béton armé
    ↓
Propriétés et dimensionnement
    ↓
Eurocode / norme / littérature technique
```

L'objectif est d'éviter que des propriétés ou formules structurales soient créées uniquement à partir d'une supposition.

### 6.8 Rapport des recherches

À la fin de l'audit, ajouter une section :

```text
## Références utilisées
```

avec les références réellement consultées.

Format recommandé :

```text
[1] Organisation / Auteur
Titre
Version / édition
Lien ou DOI
Date de consultation si pertinente
```

Pour une documentation web :

```text
[1] Nom du projet
Titre de la page
URL
Version concernée
```

Pour un livre :

```text
[2] Auteur
Titre du livre
Édition
Éditeur
Chapitre / section
```

Pour une norme :

```text
[3] Organisme
Nom de la norme
Partie
Section / article
```

### 6.9 Règle importante

**Ne pas utiliser une recherche externe pour remplacer l'analyse du code existant.**

Pour TSA, l'ordre doit être :

```text
1. Analyser le code existant
        ↓
2. Comprendre l'architecture actuelle
        ↓
3. Identifier le problème
        ↓
4. Rechercher les solutions et références pertinentes
        ↓
5. Comparer les solutions
        ↓
6. Proposer une modification minimale
        ↓
7. Attendre mon approbation
        ↓
8. Modifier le code
        ↓
9. Compiler et tester
        ↓
10. Documenter les résultats et références
```

**La recherche externe complète l'analyse du projet ; elle ne doit pas la remplacer.**

---

## 7. Règle Globale — Traçabilité des Décisions

Chaque modification architecturale importante doit pouvoir répondre à trois questions :

```text
Pourquoi cette modification ?
        ↓
Sur quelle analyse ou référence repose-t-elle ?
        ↓
Quel impact a-t-elle sur TSA ?
```

L'objectif est que plusieurs mois plus tard, un développeur puisse comprendre :

```text
Pourquoi cette classe existe ?
Pourquoi cette bibliothèque est utilisée ?
Pourquoi cette architecture a été choisie ?
Pourquoi cette méthode OCCT est utilisée ?
Pourquoi cette structure de données est organisée ainsi ?
```

sans devoir deviner les raisons originales.

---

## 8. Règle Finale — Audit + Recherche + Approbation

Le processus obligatoire est donc :

```text
┌──────────────────────────┐
│ Analyse du projet actuel │
└────────────┬─────────────┘
             ↓
┌──────────────────────────┐
│ Identification problèmes │
└────────────┬─────────────┘
             ↓
┌──────────────────────────┐
│ Recherche de solutions   │
│ + sources vérifiables    │
└────────────┬─────────────┘
             ↓
┌──────────────────────────┐
│ Comparaison              │
│ des solutions            │
└────────────┬─────────────┘
             ↓
┌──────────────────────────┐
│ Proposition              │
│ d'architecture           │
└────────────┬─────────────┘
             ↓
        ⛔ STOP
             ↓
┌──────────────────────────────┐
│ Approbation de l'utilisateur │
└────────────┬─────────────────┘
             ↓
┌──────────────────────────┐
│ Modification du code     │
└────────────┬─────────────┘
             ↓
┌──────────────────────────┐
│ Compilation + tests      │
└────────────┬─────────────┘
             ↓
┌──────────────────────────┐
│ Rapport final            │
│ + références             │
└──────────────────────────┘
```

**IMPORTANT : l'audit initial reste strictement en lecture seule.**

Avant mon approbation :

* ne modifier aucun fichier ;
* ne créer aucune classe ;
* ne créer aucune bibliothèque ;
* ne supprimer aucun code ;
* ne déplacer aucun fichier ;
* ne modifier aucun CMake ;
* ne modifier aucune fenêtre ;
* ne modifier aucune configuration ;
* ne modifier aucune partie de `TsaLib`.

Le premier objectif est de **comprendre TSA avant de le modifier**.

---

## 9. Suggestions Complémentaires (proposées — à valider)

Ces points ne remplacent rien de ce qui précède ; ce sont des ajouts possibles, cohérents avec les règles 1 à 8 et alignés sur l'architecture réelle du dépôt (`src/ExtensionSystem`, `Extensions/TSALib`, format `.tsa`, suite `TSA_TestSuite`). À valider avant intégration définitive.

### 9.1 Non-régression sur la suite de tests existante

Le dépôt contient une suite de **48 bancs d'essais** (`TSA_Tests` / `TSA_TestSuite.exe`, 48/48 PASS selon le README). Il serait cohérent d'ajouter une règle explicite :

```text
Toute modification de src/Model, src/Geometry, src/IO ou src/ExtensionSystem
    ↓
Recompilation de la cible TSA_Tests
    ↓
Exécution complète de TSA_TestSuite.exe
    ↓
48/48 PASS requis avant de considérer la tâche terminée
    ↓
Si un test échoue : corriger avant de continuer, ne pas désactiver le test
```

### 9.2 Compatibilité du format binaire `.tsa`

Le format `.tsa` (Magic `TSAF`, chunks FourCC, CRC32, spec 1.0) est un format de fichier persistant pour l'utilisateur final. Toute évolution devrait suivre une règle de compatibilité explicite :

```text
Ajout d'un nouveau CHUNK_XXX
    ↓
Compatibilité ascendante : un ancien lecteur doit pouvoir ignorer le chunk inconnu
    ↓
Compatibilité descendante : un fichier ancien doit rester chargeable
    ↓
Incrémenter Minor (ajout non cassant) ou Major (changement cassant) selon TSA_FILE_FORMAT.md
    ↓
Mise à jour de docs/TSA_FILE_FORMAT.md en conséquence
```

### 9.3 Extensions TSALib : validation et sécurité

`TSALib` étant 100% découplé (JSON/PNG, hot reload, packaging `.tsalib` signé SHA-256, protection anti-Path-Traversal selon le README), toute modification touchant `src/ExtensionSystem` ou `Extensions/TSALib` devrait explicitement documenter :

```text
Impact sur la Registry / Loader / Validator / Cache / Packager
    ↓
Impact sur la validation globale (schéma JSON, cohérence Eurocodes)
    ↓
Impact sur la sécurité (signature, anti-Path-Traversal)
    ↓
Impact sur le hot reload (modèle 3D, listes UI)
```

### 9.4 Undo/Redo comme infrastructure partagée

Le dossier `src/UndoRedo` et `src/Commands` existent déjà comme infrastructure commune. Il serait utile de formaliser, comme extension de la règle 6.4 :

```text
Toute nouvelle opération modifiant le Model
    ↓
Doit-elle passer par le système de Commands existant (src/Commands) ?
    ↓
Si non : justification explicite requise (pourquoi Undo/Redo n'est pas applicable)
```

### 9.5 Diagnostics et télémétrie

`src/Diagnostics` gère logs, télémétrie et rapports de crash. Une règle pourrait préciser :

```text
Toute nouvelle fonctionnalité risquée (calcul, I/O, parsing)
    ↓
Ajout de logs de diagnostic pertinents (succès/échec, contexte)
    ↓
Pas de données sensibles dans les logs
```

### 9.6 Journal des décisions centralisé

Plutôt que de laisser chaque décision (format de la règle 6.4) dispersée dans les rapports d'audit ponctuels, un fichier unique (par ex. `DECISIONS.md`) pourrait centraliser, dans l'ordre chronologique, toutes les décisions au format `Décision / Pourquoi / Source / Référence / Impact`. Cela répond directement à l'objectif de la règle 7 (qu'un développeur comprenne, des mois plus tard, sans deviner) et complète les documents d'audit déjà présents (`doc/TSALib_Architecture_Audit.md`).

### 9.7 Convention de commit reliée aux décisions

```text
[TSA][Module] Résumé court

Décision: ...
Source: ...
Réf: ...
```

Ceci relie directement l'historique Git au format de décision de la règle 6.4, sans dupliquer l'information dans un fichier séparé.

### 9.8 Rappel du périmètre « lecture seule » en cas d'ambiguïté

Préciser explicitement ce qui compte comme une « modification » interdite avant approbation :

```text
Autorisé pendant l'audit :
- lecture de fichiers
- exécution de commandes non destructives (grep, list, build de vérification sans écriture)
- recherche externe

Interdit avant approbation :
- toute écriture disque
- toute commande git modifiant l'historique ou l'état du dépôt
- toute régénération de fichiers CMake/projet
```

---

## 10. Règle Globale — Interconnexion Obligatoire de Tous les Éléments

**Principe fondamental** : les différentes fonctionnalités de TSA ne doivent pas fonctionner comme des systèmes indépendants possédant chacun leurs propres données, propriétés, commandes ou représentations. Une même information doit rester cohérente depuis sa création jusqu'à son affichage, sa modification, sa sauvegarde et son utilisation dans le calcul :

```text
Bibliothèques
      ↕
Propriétés
      ↕
Fenêtres Qt
      ↕
Commandes
      ↕
Modèle TSA
      ↕
Sélection
      ↕
Undo / Redo
      ↕
Save / Load
      ↕
OCCT / 3D
      ↕
Calcul / Analysis
```

**Mais attention : INTERCONNECTÉ ≠ DÉPENDANCES DIRECTES PARTOUT.**

Ne pas créer de dépendances circulaires, par exemple :

```text
Beam → Window → Model → OCCT → Beam → Window
```

ou :

```text
TsaLib ↔ UI ↔ Model ↔ TsaLib
```

Tous les systèmes doivent communiquer à travers les interfaces, modèles, services, commandes et événements déjà présents dans l'architecture TSA — jamais par des raccourcis directs entre couches qui ne devraient pas se connaître.

### 10.1 Une source de vérité

Une donnée structurale ne doit jamais exister sous plusieurs formes contradictoires. Par exemple, pour un câble `Diamètre = 30 mm`, `Matériau = Acier`, ces informations doivent rester cohérentes tout au long de la chaîne :

```text
TsaLib
   ↓
Property System
   ↓
Cable Model
   ↓
Qt Window
   ↓
OCCT
   ↓
Analysis
   ↓
Save / Load
```

Il ne doit jamais être possible d'avoir :

```text
Qt        → Ø30
Model     → Ø20
OCCT      → Ø20
Analysis  → Ø25
```

Toutes les représentations doivent dériver d'une information cohérente.

### 10.2 Les fenêtres ne doivent pas être des systèmes indépendants

Une fenêtre Qt ne doit pas posséder sa propre base de données locale. Ne pas faire :

```text
BeamWindow   → liste locale de sections
CableWindow  → autre liste locale de sections
ColumnWindow → troisième liste locale
```

mais plutôt :

```text
             TsaLib
                ↓
        Property / Library System
          ↙      ↓       ↘
      Beam     Column    Cable
      Window    Window    Window
```

Chaque fenêtre utilise les données centrales appropriées.

### 10.3 Chaque élément doit être connecté au modèle

Lorsqu'un utilisateur crée un câble (`Point A → Point B`), l'objet ne doit pas être créé uniquement dans OCCT. Le workflow doit être :

```text
Cable Tool
     ↓
Geometry Input
     ↓
Create Command
     ↓
Cable Model
     ↓
Properties
     ↓
Library
     ↓
OCCT Geometry
     ↓
Viewport
     ↓
Selection
     ↓
Property Window
```

Ainsi, sélectionner le câble dans le viewport permet de retrouver exactement le même objet dans le modèle.

### 10.4 Un élément doit être indépendant mais interconnecté

Un élément (par exemple `Cable`) possède ses propres classe, propriétés, section, matériau, fenêtre de propriétés, workflow de création et représentation OCCT — il ne doit pas être transformé en un autre type (`Beam`) simplement parce que celui-ci est déjà implémenté. Il doit cependant utiliser les infrastructures communes de TSA :

```text
Cable
  ↓
Common Model Infrastructure
  ↓
Commands
  ↓
Selection
  ↓
Undo / Redo
  ↓
Save / Load
  ↓
OCCT
  ↓
Analysis
```

Donc : **propriétés spécifiques ≠ infrastructure indépendante.**

### 10.5 Une modification doit traverser le système

Si `Cable Ø20` devient `Cable Ø30`, la modification doit suivre :

```text
Qt Property Window
       ↓
Command
       ↓
Cable Model
       ↓
Property System
       ↓
OCCT Update
       ↓
Viewport Update
       ↓
Selection Update
       ↓
Analysis Data Update
```

et Undo doit pouvoir revenir à `Cable Ø20` sans créer un deuxième système parallèle.

### 10.6 Copier/coller doit rester interconnecté

Si l'on copie un `Cable Ø30`, la copie doit conserver `Type`, `Section`, `Material`, `Properties`, `Geometry`, `ID / références nécessaires`, et rester correctement connectée à `Model`, `Library`, `Selection`, `OCCT`, `Undo/Redo`, `Save/Load`, `Analysis`. Même principe pour `Beam`, `Column`, `Bar`, `Surface`, `Cable`.

### 10.7 La sélection doit être connectée à tout le reste

Si l'on sélectionne un objet dans OCCT :

```text
OCCT Selection
      ↓
Object ID
      ↓
TSA Model
      ↓
Object Properties
      ↓
Qt Property Window
```

La fenêtre doit afficher les propriétés de **l'objet réellement sélectionné**. Elle ne doit jamais afficher un objet ou des propriétés provenant d'une copie locale.

### 10.8 Undo/Redo doit être connecté au modèle

Une modification doit passer par le système de commandes existant :

```text
User Action → Command → Model → OCCT
```

Puis :

```text
Ctrl + Z → Undo Command → Model restored → OCCT updated → UI updated
```

Ne pas créer `Cable Undo System`, `Beam Undo System`, `Column Undo System` séparés si TSA possède déjà un système général de commandes/Undo (cohérent avec la règle 9.4).

### 10.9 Save/Load doit rester connecté

Lorsqu'un modèle est sauvegardé, il doit conserver les informations nécessaires pour reconstruire correctement `Elements`, `Properties`, `Library references`, `Materials`, `Sections`, `Geometry`, `Relationships`. Après chargement :

```text
File → Model → Libraries → Properties → OCCT → UI → Analysis
```

Le modèle chargé doit redevenir un modèle TSA complet, pas simplement une géométrie OCCT (cohérent avec la règle 9.2 sur le format `.tsa`).

### 10.10 OCCT ne doit pas devenir le modèle principal

OCCT doit représenter graphiquement le modèle :

```text
TSA Model → Geometry Representation → OCCT → Viewport
```

et non l'inverse (`OCCT → propriétés → model`). Les propriétés structurales appartiennent au modèle TSA ; OCCT fournit uniquement la représentation géométrique et graphique.

### 10.11 Le calcul doit utiliser les mêmes données

Le calcul ne doit pas recréer son propre modèle contradictoire :

```text
TSA Model → Analysis Model → Solver
```

et non `UI Model → données A`, `OCCT → données B`, `Analysis → données C` en parallèle. Par exemple, un `Cable Model` avec `Diameter = 30 mm`, `Material = Steel` doit fournir au calcul exactement ces propriétés.

### 10.12 Les bibliothèques doivent être connectées

Les bibliothèques (`TsaLib` : Sections, Materials, Profiles, Cables, Supports, Loads, ...) ne doivent pas être de simples fichiers isolés. Les systèmes qui les utilisent doivent passer par les bibliothèques centrales :

```text
Section Library
       ↓
 ┌─────┼─────┐
 ↓     ↓     ↓
Beam  Bar   Cable
```

Chaque élément ne récupère que les catégories qui lui sont applicables.

### 10.13 Toute nouvelle fonctionnalité doit s'intégrer au système

Ne pas simplement créer `NewFeature.cpp`, `NewFeatureWindow.cpp`, `NewFeatureData.cpp` et laisser la fonctionnalité fonctionner seule. Il faut déterminer comment elle s'intègre à `Model`, `Library`, `Properties`, `Commands`, `Selection`, `Undo/Redo`, `Save/Load`, `OCCT`, `Analysis`, `UI`. **Une fonctionnalité n'est considérée comme terminée que lorsqu'elle est correctement intégrée aux systèmes concernés.**

### 10.14 Chercher ce qui existe déjà avant de créer

Avant de créer une classe, une bibliothèque, un manager, une fenêtre, un système de propriétés, un système Undo, une commande ou une solution graphique, vérifier d'abord si TSA possède déjà quelque chose de réutilisable (cohérent avec les règles 1 et 2). Si une solution externe est envisagée, rechercher également les solutions existantes et leurs documentations, avec des références vérifiables (documentation officielle, livre technique, norme, article scientifique, dépôt officiel — règle 6) : **ne jamais inventer une référence ou une URL** (règle 6.3).

### 10.15 Le but final

TSA doit fonctionner comme **un seul système cohérent** :

```text
             ┌───────────────┐
             │    TsaLib     │
             └───────┬───────┘
                     ↓
             ┌───────────────┐
             │   Properties  │
             └───────┬───────┘
                     ↓
             ┌───────────────┐
             │     Model     │
             └───────┬───────┘
                     ↓
        ┌────────────┼────────────┐
        ↓            ↓            ↓
   Commands      Selection     Analysis
        ↓            ↓            ↓
        └────────────┼────────────┘
                     ↓
                Undo / Redo
                     ↓
                Save / Load
                     ↓
                  OCCT
                     ↓
                Viewport
```

Les fenêtres Qt se connectent aux systèmes appropriés.

### 10.16 Règle fondamentale

Lorsque tu travailles sur TSA, pose toujours cette question :

> **« Cette modification est-elle correctement connectée au reste du système ? »**

Si la réponse est non, la fonctionnalité n'est pas encore correctement intégrée. L'objectif n'est pas seulement que chaque fonctionnalité fonctionne individuellement, mais que :

**Bibliothèques + Propriétés + UI + Modèle + Commandes + Sélection + Undo/Redo + Save/Load + OCCT + Calculs**

fonctionnent ensemble avec **une source de vérité cohérente, des interfaces claires, peu de couplage inutile et aucune duplication contradictoire**.

L'interconnexion est donc une **règle architecturale permanente de TSA**, et non une correction ponctuelle limitée au système Cable.

### 10.17 Traçabilité croisée entre artefacts (complément)

Au-delà de l'architecture logicielle (10.1 à 10.16), le même principe d'interconnexion s'applique aux artefacts du projet : règle (AGENTS.md), décision (règle 6.4), documentation (`docs/`, `doc/`, `README.md`), code, test (`TSA_TestSuite`) et commit Git (règle 9.7) doivent rester reliés entre eux plutôt qu'exister isolément :

```text
Règle (AGENTS.md) ↕ Décision ↕ Documentation ↕ Code ↕ Test ↕ Commit Git
```

Avant de considérer une tâche terminée :

```text
Le code a-t-il une décision associée (règle 6.4) ?
        ↓
La décision référence-t-elle précisément le code (fichier/classe/fonction) ?
        ↓
Le changement est-il couvert par un test (règle 9.1) ?
        ↓
La documentation concernée a-t-elle été mise à jour si nécessaire ?
        ↓
Le commit référence-t-il la décision (règle 9.7) ?
```

Si l'une de ces questions n'a pas de réponse claire, l'élément manquant doit être ajouté avant de considérer la tâche terminée.

---

## 11. Règle Importante — Aucun Doublon de Commande

Il faut absolument éviter de créer plusieurs commandes qui réalisent la même opération.

Avant de créer une nouvelle commande, rechercher dans tout le projet si une commande existante permet déjà de réaliser cette opération ou peut être généralisée proprement.

Par exemple, ne pas créer :

```text
CreateBeamCommand
CreateCableCommand
CreateColumnCommand
CreateBarCommand
```

si le système actuel possède déjà une infrastructure générique permettant de gérer la création des éléments.

De même, éviter de créer plusieurs commandes indépendantes pour :

```text
Move
Delete
Copy
Paste
ModifyProperties
ChangeSection
ChangeMaterial
CreateElement
```

lorsqu'une commande existante peut être réutilisée ou étendue correctement.

### 11.1 Principe

Les commandes doivent suivre la même philosophie que les bibliothèques :

```text
UNE OPÉRATION
      ↓
UNE LOGIQUE DE COMMANDE
      ↓
RÉUTILISABLE PAR PLUSIEURS ÉLÉMENTS
```

Les différences spécifiques à chaque élément doivent être gérées par les données ou comportements spécifiques de l'élément, et non par la duplication complète de la commande.

Par exemple :

```text
             CreateElementCommand
                     │
          ┌──────────┼──────────┐
          ↓          ↓          ↓
        Beam       Column      Cable
```

Cela ne signifie pas que tous les éléments doivent obligatoirement utiliser exactement la même classe de commande. Si un élément possède réellement un workflow différent, il peut avoir une commande spécialisée. Mais avant de créer `CreateCableCommand`, il faut vérifier si `CreateElementCommand` ou une commande existante peut être utilisée ou adaptée proprement.

### 11.2 Même règle pour Undo/Redo

Une opération ne doit pas avoir plusieurs systèmes Undo concurrents. Éviter :

```text
BeamUndo
CableUndo
ColumnUndo
```

si TSA possède déjà un système global de commandes et Undo/Redo (cohérent avec les règles 9.4 et 10.8). Le workflow doit rester :

```text
User Action
     ↓
Command
     ↓
Model
     ↓
OCCT
     ↓
Undo / Redo
```

### 11.3 Même règle pour les autres fonctionnalités

Rechercher les doublons dans :

* commandes ;
* managers ;
* services ;
* propriétés ;
* bibliothèques ;
* validation ;
* sélection ;
* création géométrique ;
* mise à jour OCCT ;
* sauvegarde ;
* chargement ;
* conversion de données ;
* notifications ;
* gestion des événements.

Pour chaque doublon trouvé, ne pas supprimer immédiatement. D'abord identifier :

```text
Nom
Emplacement
Responsabilité
Utilisateurs
Fonctionnalité
Source de vérité
Doublon potentiel
```

Puis déterminer s'il s'agit réellement d'un doublon ou d'une responsabilité volontairement spécialisée.

### 11.4 Règle architecturale

**Ne jamais résoudre un problème en créant simplement une deuxième implémentation parallèle.**

Avant de créer quelque chose de nouveau :

```text
1. Rechercher l'existant
        ↓
2. Comprendre son rôle
        ↓
3. Vérifier s'il peut être réutilisé
        ↓
4. Vérifier s'il peut être généralisé
        ↓
5. Vérifier les conséquences sur les autres systèmes
        ↓
6. Seulement ensuite créer une nouvelle implémentation si nécessaire
```

L'objectif est que TSA possède **une architecture intégrée et réutilisable**, et non plusieurs systèmes parallèles qui finissent par diverger.

### 11.5 Exemple concret

Ne pas arriver progressivement à :

```text
BeamCommandSystem
CableCommandSystem
ColumnCommandSystem
SurfaceCommandSystem
```

avec chacun :

```text
Create
Delete
Move
Copy
Paste
Modify
Undo
Redo
```

et donc plusieurs implémentations de la même logique. Préférer :

```text
             TSA Command System
                     │
       ┌─────────────┼─────────────┐
       ↓             ↓             ↓
   Generic       Element       Specialized
   Commands      Commands       Commands
       │             │             │
       └─────────────┼─────────────┘
                     ↓
                   Model
```

avec une spécialisation uniquement lorsqu'elle est réellement nécessaire.

### 11.6 Principe final

> Avant de créer une nouvelle commande, chercher d'abord si la commande existe déjà.
>
> Avant de créer un nouveau système, chercher d'abord si le système existe déjà.
>
> Avant de créer une nouvelle donnée, chercher d'abord si une source de vérité existe déjà.

Cela doit être appliqué à toute l'architecture TSA, en cohérence avec les règles 10.14 et 6.6.

---

## 12. Règle — Classification et Organisation des Commandes

Le système de commandes de TSA doit être clairement structuré et correctement classifié, avec une organisation inspirée du principe de classification des commandes utilisé dans des logiciels de CAO comme AutoCAD.

**L'objectif n'est PAS de copier le code ou l'architecture interne d'AutoCAD**, mais de reprendre le principe important suivant : **chaque commande doit appartenir à une catégorie fonctionnelle clairement identifiable.**

Avant de créer ou modifier une commande, analyser les commandes déjà présentes dans TSA et déterminer leur classification.

### 12.1 Classification fonctionnelle

Les commandes doivent être organisées par grandes catégories, par exemple :

* **Création / Draw** — Create Beam, Create Column, Create Bar, Create Cable, Create Surface, Create Node, etc.
* **Modification / Modify** — Move, Rotate, Scale, Stretch, Offset, Trim, Extend, Mirror, Edit Properties, etc.
* **Copie / Duplication** — Copy, Array, Duplicate, Paste, etc.
* **Suppression / Delete** — Delete Element, Delete Node, Delete Surface, etc.
* **Sélection / Selection** — Select, Select Similar, Select by Type, Select by Property, etc.
* **Propriétés / Properties** — Edit Element Properties, Assign Section, Assign Material, Assign Parameters, etc.
* **Bibliothèques / Libraries** — Add Section, Modify Section, Delete Section, Add Material, Modify Material, etc.
* **Structure / Structural** — Assign Support, Assign Load, Release, Connection, etc.
* **Analyse / Analysis** — Generate Analysis Model, Mesh, Solve, Run Analysis, etc.
* **Affichage / View** — Zoom, Pan, Rotate View, Fit View, Display Mode, Show/Hide Elements, etc.
* **Fichier / File** — New, Open, Save, Save As, Import, Export, etc.
* **Édition / Edit** — Undo, Redo, Cut, Copy, Paste, etc.

Cette classification doit rester cohérente avec l'architecture réelle de TSA.

### 12.2 Une commande = une responsabilité claire

Chaque commande doit avoir une responsabilité précise. Ne pas créer plusieurs commandes différentes qui réalisent essentiellement la même opération.

Exemple : si TSA possède déjà `CreateElementCommand`, il ne faut pas créer inutilement `CreateBeamCommand`, `CreateColumnCommand`, `CreateCableCommand`, `CreateBarCommand` si ces commandes ne font finalement que répéter la même logique (cohérent avec la règle 11.1). Dans ce cas, utiliser une commande générique avec un type d'élément ou une stratégie spécialisée lorsque cela est réellement nécessaire.

À l'inverse, si le workflow d'un `Cable` est réellement différent de celui d'une `Beam`, il peut avoir une commande spécialisée, mais cette différence doit être justifiée architecturalement (format décision, règle 6.4).

### 12.3 Classification ≠ duplication

La classification ne doit surtout pas conduire à créer plusieurs systèmes de commandes parallèles. Ne pas avoir `BeamCommandSystem`, `ColumnCommandSystem`, `CableCommandSystem`, `SurfaceCommandSystem` avec chacun ses propres Create/Delete/Move/Copy/Paste/Modify/Undo/Redo si une infrastructure commune existe déjà (cohérent avec la règle 11.4).

Architecture souhaitée :

```text
Command System
→ Command Category
→ Command
→ Element Type / Target
→ Model
→ OCCT
→ UI
```

### 12.4 Nommage cohérent

Convention de nommage cohérente, par exemple : `CreateElementCommand`, `DeleteElementCommand`, `MoveElementCommand`, `CopyElementCommand`, `ModifyElementPropertiesCommand`, `AssignSectionCommand`, `AssignMaterialCommand`. Éviter les noms incohérents ou plusieurs noms pour la même responsabilité. Avant de créer un nouveau nom, rechercher les classes et commandes existantes.

### 12.5 Commandes et UI

Les boutons, icônes, menus, raccourcis clavier et outils de la barre de commande doivent utiliser les commandes existantes — jamais implémenter directement une opération qui existe déjà dans le Command System.

Architecture souhaitée :

```text
Qt Button / Toolbar / Menu / Shortcut
↓
Command
↓
Model
↓
Property / Library
↓
OCCT
↓
Viewport
```

et non `Qt Button → OCCT directement` lorsque cela contourne le modèle et le système de commandes (cohérent avec la règle 10.10).

### 12.6 Commandes et Undo/Redo

Toutes les commandes qui modifient le modèle doivent être compatibles avec le système global Undo/Redo :

```text
MoveElementCommand → Execute → Model modification → OCCT update
```

puis :

```text
Ctrl + Z → Undo → Model restoration → OCCT synchronization → UI synchronization
```

Ne pas créer un système Undo spécifique pour chaque type d'élément si TSA possède déjà un système global (règles 9.4, 10.8, 11.2).

### 12.7 Organisation des fichiers du Command System

Organiser les fichiers et classes du Command System de manière logique, par exemple (si l'architecture existante le permet) :

```text
Commands/
├── Core/
│   ├── Command.h
│   ├── CommandManager.h
│   └── CommandHistory.h
│
├── Create/
│   ├── CreateElementCommand.h
│   ├── CreateNodeCommand.h
│   └── CreateSurfaceCommand.h
│
├── Modify/
│   ├── MoveElementCommand.h
│   ├── RotateElementCommand.h
│   └── ModifyPropertiesCommand.h
│
├── Edit/
│   ├── CopyCommand.h
│   ├── PasteCommand.h
│   ├── UndoCommand.h
│   └── RedoCommand.h
│
├── Selection/
│   └── SelectionCommands.h
│
├── Library/
│   ├── AddSectionCommand.h
│   └── AddMaterialCommand.h
│
└── Analysis/
    └── RunAnalysisCommand.h
```

**Mais attention** : ne pas créer automatiquement cette structure si TSA possède déjà une organisation différente et cohérente (`src/Commands` existe déjà, cf. règle 9.4). Commencer par analyser l'architecture actuelle et déterminer comment améliorer la classification sans créer de doublons ni provoquer une refactorisation inutile.

### 12.8 Audit obligatoire avant création d'une commande

Avant de créer une nouvelle commande :

1. rechercher les commandes existantes ;
2. rechercher les fonctions existantes ;
3. rechercher les Command Managers existants ;
4. rechercher les opérations similaires ;
5. vérifier si une commande générique peut être réutilisée ;
6. vérifier si une commande existante peut être généralisée ;
7. vérifier les relations avec Undo/Redo ;
8. vérifier les relations avec le Model ;
9. vérifier les relations avec OCCT ;
10. déterminer sa catégorie fonctionnelle ;
11. vérifier son nommage ;
12. vérifier qu'elle ne crée pas un système parallèle.

Ensuite seulement, proposer la solution.

### 12.9 Rapport d'audit des commandes

Pendant l'audit, créer un tableau permettant de comprendre le système actuel :

| Commande | Catégorie | Responsabilité | Utilisée par | Modifie Model | Modifie OCCT | Undo/Redo | Doublon potentiel |
| -------- | --------- | -------------- | ------------ | ------------- | ------------ | --------- | ----------------- |

Pour chaque doublon potentiel, ne rien supprimer immédiatement. Expliquer :

* où se trouve le doublon ;
* pourquoi il existe ;
* quelle implémentation semble être la source de vérité ;
* qui utilise chaque implémentation ;
* quelle serait la meilleure stratégie de fusion ou généralisation ;
* quels seraient les impacts.

### 12.10 Objectif final

Le système de commandes de TSA doit être : clairement classifié, facilement navigable, extensible, réutilisable, cohérent avec le Model, cohérent avec TsaLib, cohérent avec les Property Systems, cohérent avec OCCT, compatible Undo/Redo, compatible avec les fenêtres Qt, compatible avec les raccourcis et menus, sans commandes dupliquées, sans systèmes de commandes parallèles inutiles.

Le principe général doit être :

> **Une opération fonctionnelle → une commande clairement identifiée → une catégorie claire → une infrastructure commune → une synchronisation avec le reste de TSA.**

Et surtout :

> **Ne créer aucune nouvelle commande avant d'avoir vérifié si une commande existante peut être réutilisée, généralisée ou correctement classifiée.**

---

## 13. Règle — Organisation des Fenêtres et Panneaux

L'organisation de l'interface graphique de TSA doit être pensée selon une logique similaire à celle des logiciels de CAO professionnels comme AutoCAD.

**L'objectif n'est PAS de copier l'interface d'AutoCAD pixel par pixel**, mais de reprendre son principe d'organisation : **le logiciel doit être composé de fenêtres, panneaux, palettes et barres d'outils clairement organisés, accessibles, déplaçables et personnalisables.**

### 13.1 Fenêtres dockables

Les fenêtres importantes de TSA (Properties, Project/Model Tree, Libraries, Materials, Sections, Commands/Tools, Layers ou catégories similaires, Analysis, Results, Messages/Logs, etc.) doivent pouvoir être organisées comme des panneaux dockables : dockées à gauche/droite/haut/bas, redimensionnées, déplacées, regroupées, transformées en onglets, détachées en fenêtres flottantes lorsque pertinent, masquées puis réaffichées.

Éviter une interface composée de nombreuses fenêtres indépendantes qui apparaissent de manière désordonnée.

### 13.2 Une organisation centrale de l'interface

TSA doit avoir une organisation centrale de ses fenêtres et panneaux, par exemple :

```text
┌──────────────────────────────────────────────────────────────┐
│ Menu / Ribbon / Commandes                                    │
├───────────────┬───────────────────────────────┬──────────────┤
│               │                               │              │
│ Model Tree    │                               │ Properties   │
│               │          OCCT 3D              │              │
│ Libraries     │          Viewport              │              │
│               │                               │              │
│               │                               │              │
├───────────────┴───────────────────────────────┴──────────────┤
│ Messages / Logs / Analysis / Results                          │
└──────────────────────────────────────────────────────────────┘
```

Cette disposition n'est qu'un exemple. **Il faut d'abord analyser l'interface actuelle de TSA avant de décider de la structure finale.**

### 13.3 Les fenêtres ne doivent pas être des systèmes isolés

Une fenêtre Qt ne doit pas devenir une mini-application indépendante :

```text
CablePropertiesWindow
        ↓
Property System
        ↓
Cable Model
        ↓
TsaLib
        ↓
Command System
        ↓
OCCT
```

Même principe pour `BeamPropertiesWindow`, `ColumnPropertiesWindow`, `BarPropertiesWindow`, `SurfacePropertiesWindow`, `MaterialWindow`, `SectionWindow`, `AnalysisWindow`, etc. (cohérent avec la règle 10.2). Les fenêtres sont des interfaces utilisateur permettant de manipuler le système central de TSA.

### 13.4 Fenêtre Properties contextuelle

Logique similaire aux logiciels de CAO professionnels :

```text
Sélection dans le viewport
        ↓
Identification de l'objet TSA
        ↓
Property System
        ↓
Properties Panel
```

Exemple : sélectionner un `Cable` affiche `Section = Cable Ø20`, `Material = Steel`, `Length = ...` ; sélectionner une `Beam` affiche `Section = 40 × 40`, `Material = C25/30`, etc. Le panneau Properties doit donc être **contextuel** et afficher les propriétés réelles de l'objet sélectionné (cohérent avec la règle 10.7).

### 13.5 Une fenêtre par responsabilité

Éviter de créer une fenêtre pour chaque petite fonction. Avant de créer une nouvelle fenêtre :

1. rechercher les fenêtres existantes ;
2. rechercher les panels existants ;
3. rechercher les Property Providers ;
4. rechercher les systèmes de bibliothèques ;
5. vérifier si une fenêtre existante peut être réutilisée ;
6. vérifier si un panneau contextuel peut gérer le besoin ;
7. vérifier si une nouvelle fenêtre est réellement nécessaire.

Ne pas avoir par exemple `BeamSectionWindow`, `CableSectionWindow`, `ColumnSectionWindow`, `BarSectionWindow` si toutes ces fenêtres servent simplement à sélectionner des sections provenant de la même bibliothèque centrale. Étudier plutôt :

```text
Section Library Panel
        ↓
TsaLib
        ↓
Element Property System
```

tout en conservant des propriétés spécifiques lorsque les éléments ont réellement des besoins différents.

### 13.6 Fenêtres et commandes doivent être connectées

L'organisation des fenêtres doit être cohérente avec la classification des commandes (règle 12.1) :

```text
CREATE
 ├── Beam
 ├── Column
 ├── Bar
 ├── Cable
 └── Surface

MODIFY
 ├── Move
 ├── Rotate
 ├── Copy
 └── Properties

LIBRARIES
 ├── Sections
 ├── Materials
 └── Cable Sections

ANALYSIS
 ├── Mesh
 ├── Solve
 └── Results
```

Les boutons, icônes, menus et panneaux doivent appeler les commandes du système central (règle 12.5). La fenêtre ne doit pas contenir sa propre logique parallèle de création ou de modification.

### 13.7 Organisation modifiable par l'utilisateur

Lorsque Qt le permet, l'interface doit être conçue pour permettre à l'utilisateur de personnaliser son espace de travail, par exemple :

```text
Workspace
├── Structural Modeling
├── Analysis
├── Results
├── Libraries
└── Custom
```

Un utilisateur travaillant principalement sur la modélisation peut avoir `Model Tree + Properties + 3D View` ; un utilisateur travaillant sur l'analyse peut avoir `Model Tree + Analysis + Results + 3D View`. L'objectif est de pouvoir faire évoluer l'espace de travail sans modifier le cœur du logiciel.

### 13.8 Sauvegarde de l'organisation

Si l'architecture actuelle le permet, étudier la possibilité de sauvegarder/restaurer : position des panneaux, taille des panneaux, fenêtres dockées/flottantes, onglets, workspace utilisé, panneaux visibles/cachés. Mais avant d'implémenter cela, rechercher si Qt fournit déjà les mécanismes nécessaires (`QMainWindow::saveState`/`restoreState`, etc. — règle 1). Ne pas recréer inutilement un système qui existe déjà dans Qt.

### 13.9 Recherche obligatoire avant nouvelle interface

Avant de créer une nouvelle fenêtre ou un nouveau système d'organisation :

* analyser les fenêtres Qt existantes ;
* analyser les layouts existants ;
* analyser les `QDockWidget` ;
* analyser les menus ;
* analyser les toolbars ;
* analyser les panneaux Properties ;
* analyser le système de navigation ;
* rechercher les solutions déjà présentes dans TSA ;
* rechercher les possibilités natives de Qt.

Si une solution existante peut être réutilisée, il faut la privilégier.

### 13.10 Objectif final

TSA doit avoir une interface de logiciel de CAO professionnel :

```text
Commandes
↕
Toolbars / Menus / Icônes
↕
Panneaux / Fenêtres dockables
↕
Property System
↕
Model
↕
OCCT / 3D
```

Les fenêtres doivent être des vues différentes d'un même système central, et non plusieurs systèmes indépendants. Le principe fondamental est :

> **UNE INTERFACE UNIFIÉE + DES PANNEAUX ORGANISÉS + UN MODÈLE CENTRAL + DES COMMANDES CENTRALISÉES.**

Et comme pour le système de commandes (règle 12) :

> **Avant de créer une nouvelle fenêtre, rechercher d'abord si une fenêtre, un panneau ou un système existant peut être réutilisé ou généralisé.**

Ne pas créer de fenêtres ou systèmes parallèles simplement parce qu'un nouvel élément, comme Cable, Beam ou Column, possède des propriétés différentes.

---

## 14. Règle — Une Classe Métier Distincte Peut Avoir sa Propre Fenêtre

Lorsqu'un élément structural est réellement une **classe métier distincte**, avec ses propres propriétés, paramètres, règles de modélisation et comportement, il doit disposer de sa **propre fenêtre ou de son propre panneau de propriétés adapté**.

Exemples :

```text
Beam            → BeamPropertiesWindow
Column          → ColumnPropertiesWindow
Cable           → CablePropertiesWindow
Surface / Slab  → SurfacePropertiesWindow
Wall            → WallPropertiesWindow
```

Par exemple, **Cable, Beam et Wall ne doivent pas être forcés à utiliser une seule fenêtre générique** si leurs propriétés et leurs comportements sont réellement différents. Cette règle complète la 13.5 (« une fenêtre par responsabilité ») : elle ne s'y oppose pas, elle en précise la limite — la règle 13.5 empêche les fenêtres redondantes pour une même responsabilité, celle-ci reconnaît qu'une responsabilité métier réellement différente mérite sa propre fenêtre.

### 14.1 Propre fenêtre ≠ système indépendant

Avoir une fenêtre propre ne signifie **PAS** créer un système parallèle. Chaque fenêtre doit utiliser les infrastructures communes de TSA :

```text
Element-specific Properties Window
              ↓
       Common Property System
              ↓
          TSA Model
              ↓
           TsaLib
              ↓
       Command System
              ↓
        Undo / Redo
              ↓
             OCCT
              ↓
           Viewport
```

Ainsi, `CablePropertiesWindow`, `BeamPropertiesWindow`, `WallPropertiesWindow` peuvent être différentes au niveau de leur interface et de leurs propriétés métier, tout en partageant les mêmes infrastructures centrales (cohérent avec la règle 10.4 : « propriétés spécifiques ≠ infrastructure indépendante »).

### 14.2 Exemple

```text
CablePropertiesWindow          BeamPropertiesWindow           WallPropertiesWindow
├── Cable Section               ├── Beam Section                ├── Thickness
├── Diameter                    ├── Material                    ├── Material
├── Material                    ├── Orientation                 ├── Reinforcement
├── Pretension                  ├── Releases                    ├── Orientation
├── Parameters                  └── Beam-specific properties    └── Wall-specific properties
└── Cable-specific properties
```

Il ne faut donc pas essayer de mettre artificiellement toutes ces propriétés dans une seule fenêtre universelle si cela rend le système incohérent.

### 14.3 Architecture à respecter

Le principe recherché est : **interface spécialisée + infrastructure commune.**

```text
                    Common TSA Infrastructure
                             │
        ┌────────────────────┼────────────────────┐
        ↓                    ↓                    ↓
BeamProperties       CableProperties       WallProperties
    Window                Window                Window
        ↓                    ↓                    ↓
     Beam                  Cable                 Wall
        └────────────────────┼────────────────────┘
                             ↓
                         TSA Model
                             ↓
                           OCCT
```

### 14.4 Règle de décision

Avant de créer ou fusionner une fenêtre, analyser :

1. Est-ce une classe métier distincte ?
2. Possède-t-elle des propriétés spécifiques ?
3. Possède-t-elle un workflow différent ?
4. Possède-t-elle une représentation ou des paramètres spécifiques ?
5. Existe-t-il déjà une fenêtre dédiée ?
6. Une fenêtre générique serait-elle réellement adaptée ?

Si la réponse montre que l'élément possède une identité métier propre, **conserver ou créer une fenêtre spécialisée**. En revanche, ne pas dupliquer toute l'infrastructure derrière cette fenêtre.

**Ne jamais confondre :**

> fenêtre spécialisée

avec

> système indépendant.

Le premier est souhaité lorsque les besoins métier sont différents ; le second doit être évité lorsqu'une infrastructure commune existe déjà.

Cette règle s'applique à tous les éléments de TSA, pas uniquement au Cable.