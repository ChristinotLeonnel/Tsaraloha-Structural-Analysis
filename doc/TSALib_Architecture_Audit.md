# Audit Architectural & Spécification du Système d'Extensions TSALib

**Projet :** TSA — Tsaraloha Structural Analysis  
**Composant :** TSALib — TSA Engineering Library & Extension System  
**Auteur :** Tsaraloha Christinot  
**Date :** 27 Septembre 2026  
**Statut :** Phase 1 — Audit complet & Fondation des contrats de données  

---

## Évaluation Préalable Obligatoire (Directives de Développement TSA)

```text
Besoin :
Système professionnel d'extensions de bibliothèques techniques (matériaux, sections, profilés métalliques, câbles, armatures, précontrainte, textures PBR, normes Eurocodes) découplé du binaire exécutable TSA.exe, garantissant l'ajout et la mise à jour de données sans recompilation, avec validation stricte, gestion de versions (SemVer 2.0), snapshots immuables pour la reproductibilité des calculs, et rechargement dynamique à chaud.

Solution existante trouvée :
1. Sérialisation & Schémas JSON : Qt6::Core (QJsonDocument, QJsonObject, QJsonArray, QJsonValue).
2. Traitement d'images / textures : Qt6::Gui (QImage, QPixmap) & OpenCASCADE Image_AlienPixMap / Graphic3d_Texture2D.
3. Intégrité & Hash : QCryptographicHash (SHA-256) intégré à Qt.
4. Parsing SemVer & Expressions régulières : QRegularExpression.

Source :
- Documentation officielle Qt 6 (Qt Core, Qt Gui)
- Documentation OpenCASCADE V8.0.1 (Graphic3d, AIS, BRepPrimAPI)
- Spécification Semantic Versioning 2.0.0 (semver.org)

Compatibilité TSA :
- Standard C++20 natif
- Toolchains Windows MSVC (Visual Studio 2022) & MinGW-w64
- Qt 6.2+ (actuellement configuré sur Qt 6.11.2)
- Architecture x64 Windows 10/11
- Intégration CMake native sans ajout de DLL tierce superflue

Avantages :
- Zéro dépendance externe supplémentaire : s'appuie sur le framework Qt6 et OCCT déjà linkés et validés dans TSA.
- Performance mémoire maximale, absence de fuite (RAII), support natif de l'UTF-8 et des chemins Windows longs.
- Robustesse face aux fichiers JSON corrompus ou tronqués (QJsonParseError).

Limites :
- Les bibliothèques génériques du web ne connaissent pas les concepts métier du génie civil (Eurocodes, calculs EF, sections métalliques standardisées, modèles mécaniques de câbles).

Solution personnalisée nécessaire :
Oui pour l'architecture métier d'ingénierie structurale (ExtensionManager, LibraryManager, LibraryRegistry, LibraryLoader, LibraryValidator, LibraryCache, LibraryVersionManager, MechanicalSnapshot), s'appuyant sur les primitives robustes de Qt6::Core.
```

---

## 1. Architecture Actuelle Détectée

L'audit approfondi du code source de TSA a mis en évidence l'architecture modulaire suivante :

```text
TSA
├── App/                → Cycle de vie de l'application Qt (Application.h/.cpp)
├── Core / Diagnostics/ → Logging asynchrone, RingBuffer, CrashHandler, DiagnosticReport
├── Coordinate/         → Repères 3D cartésiens et cylindriques, gestionnaire d'étages (LevelManager)
├── Grid/               → Lignes et grilles de construction 3D (CartesianGrid, CylindricalGrid, ArbitraryGrid, GridSnapManager)
├── Model/              → Source de vérité structurale :
│   ├── Node, Beam, Column, Slab, Wall, Foundation, TrussMember
│   ├── Cable/ (Cable, CableDefinition, CableStandards, CableAnchor, StayCable, SuspensionSystem)
│   ├── Material, Section
│   └── Model, ModelDiff, StructuralClipboard
├── Geometry/           → Génération de solides B-Rep OCCT réels (BeamGeometry, SlabGeometry, WallGeometry, CableGeometry3D)
├── Viewer/             → Rendu 3D interactif OpenCASCADE (OccView, MaterialVisual, SelectionManager)
├── IO/                 → Sérialisation binaire par Chunks (.tsa) (TSAFile, TSAFileFormat, TSAPreviewGenerator)
├── Project/            → Gestionnaire de projet de haut niveau (ProjectManager)
├── Commands/ & Undo/   → Patron Commande (UndoManager, CommandManager, CreateBeamCommand, GridCommands)
├── Library/            → Bibliothèques actuelles (LibraryManager, CableLibrary)
└── UI/                 → Interface graphique Qt (MainWindow, RibbonBar, PropertyPanel, ModelTreeWidget, Dialogues spécialisés)
```

---

## 2. Systèmes de Bibliothèques Déjà Présents

Trois sous-systèmes partiels et hétérogènes coexistent aujourd'hui dans TSA :

1. **`TSA::Model::MaterialLibrary` (`src/Model/MaterialLibrary.h/.cpp`)** :
   - Singleton fournissant des matériaux Eurocodes codés en dur en C++ (`concreteC25_30`, `steelS235`, `timberC24`, etc.).
   - Possède une liste locale `m_customMaterials` en mémoire vive.
   - Utilisé par `PropertyPanel` pour peupler les listes déroulantes de matériaux.

2. **`TSA::Library::LibraryManager` (`src/Library/LibraryManager.h/.cpp`)** :
   - Singleton stockant dans `QStandardPaths::AppLocalDataLocation + "/Library"` des fichiers JSON pour les sections, matériaux, couleurs, textures et modèles de structures.
   - Initialise les sections standards en appelant `TSA::Model::Section::defaultLibrary()` et les matériaux via `TSA::Model::Material::defaultLibrary()`, tous deux compilés en dur dans l'exécutable.
   - Utilisé par `LibraryDialog` (Dialogue "Bibliothèque...") et `BarCreationDialog`.

3. **`TSA::Library::CableLibrary` (`src/Library/CableLibrary.h/.cpp`)** :
   - Singleton dédié aux câbles (`CableDefinition`).
   - Initialise ses définitions standards depuis `CableStandards.cpp` (produits normés EN 1993-1-11, torons Y1860S7, etc. compilés en dur).
   - Sauvegarde les câbles personnalisés dans un fichier JSON `cables_custom.json`.

---

## 3. Duplications Détectées

L'audit a révélé des duplications critiques qui compromettent la cohérence des données :

1. **Désynchronisation Matériaux (`MaterialLibrary` vs `LibraryManager`)** :
   - `TSA::Model::MaterialLibrary` maintient son propre vecteur de matériaux standards et personnalisés.
   - `TSA::Library::LibraryManager` maintient indépendamment un autre vecteur de matériaux standards et personnalisés.
   - **Conséquence directe :** Lorsqu'un utilisateur créait un matériau personnalisé dans le dialogue `LibraryDialog`, il était ajouté dans `TSA::Library::LibraryManager`, mais `PropertyPanel` interrogeait exclusivement `TSA::Model::MaterialLibrary`. Le nouveau matériau n'apparaissait donc jamais dans les propriétés des éléments !

2. **Multiplication des définitions de Sections standards** :
   - `TSA::Model::Section::defaultLibrary()` contient la liste C++ des profilés IPE/HEA/HEB.
   - `TSA::Library::LibraryManager::populateDefaultStandardData()` duplique ces objets.
   - `BarCreationDialog::populateSections()` rappelle encore directement `Section::defaultLibrary()`.

3. **Redondance des Câbles** :
   - `TSA::Model::CableDefinition::defaultLibrary()` compile en dur les câbles.
   - `TSA::Library::CableLibrary` maintient une copie redondante dans `m_standardDefinitions`.

---

## 4. Fichiers à Modifier

Les composants suivants seront migrés pour consommer le nouveau système unifié `ExtensionSystem` :

1. **`CMakeLists.txt`** : Intégration de la cible `ExtensionSystem` dans `SOURCES` et `TEST_SOURCES`.
2. **`src/Model/MaterialLibrary.h/.cpp`** : Transformation en adaptateur léger redirigeant vers `LibraryRegistry`.
3. **`src/Library/LibraryManager.h/.cpp`** : Redirection de la gestion des données vers le nouveau moteur d'extensions tout en maintenant la compatibilité des dialogues existants.
4. **`src/Library/CableLibrary.h/.cpp`** : Redirection des requêtes de catalogue vers `LibraryRegistry`.
5. **`src/UI/Properties/PropertyPanel.cpp`** : Récupération dynamique des matériaux et profilés depuis `LibraryRegistry`.
6. **`src/UI/Dialogs/BarCreationDialog.cpp`** : Alimentation des menus déroulants de profilés et matériaux via `LibraryRegistry`.
7. **`src/UI/Dialogs/LibraryDialog.h/.cpp`** : Connexion de l'UI au rechargement dynamique et à la gestion des extensions.
8. **`src/Viewer/MaterialVisual.cpp`** : Résolution des textures physiques à partir du répertoire externe `Extensions/TSALib/Textures/` via le cache sans recompilation.
9. **`src/IO/TSAFile.cpp` & `src/IO/TSAFileFormat.h`** : Sauvegarde des références `libraryId`, `definitionId`, `version` et du snapshot mécanique immuable `MechanicalSnapshot`.

---

## 5. Nouveaux Fichiers Créés et à Créer

### Arborescence Système C++ (`src/ExtensionSystem/`)
- `ExtensionTypes.h` / `ExtensionTypes.cpp` *(Créé en Phase 1)* : Types fondamentaux (SemVer 2.0, Manifest, Snapshot, Unités physiques SI, Catégories, ValidationResult).
- `ExtensionManager.h` / `ExtensionManager.cpp` *(Phase 2)* : Découverte des extensions sur disque, gestionnaire du cycle de vie.
- `LibraryManager.h` / `LibraryManager.cpp` *(Phase 2)* : Contrôleur centralisé des bibliothèques d'ingénierie.
- `LibraryRegistry.h` / `LibraryRegistry.cpp` *(Phase 2)* : Registre logique des définitions indexées par ID logique.
- `LibraryLoader.h` / `LibraryLoader.cpp` *(Phase 2)* : Moteur de parsing JSON asynchrone / lazy loading.
- `LibraryValidator.h` / `LibraryValidator.cpp` *(Phase 2)* : Moteur de validation stricte (schéma, intégrité, cohérence d'unités).
- `LibraryCache.h` / `LibraryCache.cpp` *(Phase 2)* : Cache haute performance en mémoire vive.
- `LibraryVersionManager.h` / `LibraryVersionManager.cpp` *(Phase 2)* : Comparaison sémantique de versions et diff de propriétés.
- `LibraryDependencyManager.h` / `LibraryDependencyManager.cpp` *(Phase 2)* : Résolution des graphes de dépendances.

### Arborescence sur Disque (`Extensions/TSALib/`)
- `Extensions/TSALib/manifest.json` *(Phase 3)*
- `Extensions/TSALib/Materials/` *(Phase 4)* (Concrete, Steel, Timber, Masonry, Soil, Glass, Aluminum)
- `Extensions/TSALib/Textures/` *(Phase 5)* (Textures PBR externes : albedo, normal, roughness)
- `Extensions/TSALib/Sections/` & `Profiles/` *(Phase 6)* (Profilés IPE, HEA, HEB, UPN, tubes, etc.)
- `Extensions/TSALib/Cables/` *(Phase 7)* (Torons, haubans, câbles de suspension, barres)
- `Extensions/TSALib/Standards/` *(Phase 3/4)* (Fiches normatives EN 1990 à 1999)

---

## 6. Plan de Migration en 11 Phases

| Phase | Intitulé | Objectif & Livrable |
|---|---|---|
| **Phase 1** | **Audit complet & Fondation des types** | Diagnostic architectural, identification des doublons, création de `ExtensionTypes.h/.cpp` et validation par tests unitaires. *(Achevé)* |
| **Phase 2** | **Noyau ExtensionSystem** | Implémentation de `ExtensionManager`, `LibraryManager`, `LibraryRegistry`, `LibraryLoader`, `LibraryValidator`, `LibraryCache`, `LibraryVersionManager`, `LibraryDependencyManager`. |
| **Phase 3** | **Manifest & Structure TSALib** | Spécification formelle de `manifest.json`, schéma JSON et mise en place du dossier `Extensions/TSALib/`. |
| **Phase 4** | **Externalisation des Matériaux** | Migration des matériaux Eurocodes vers des fichiers JSON externes, découplage total du binaire C++. |
| **Phase 5** | **Externalisation des Textures** | Gestionnaire de textures externes pour le rendu PBR OpenCASCADE (`MaterialVisual`), rechargement à chaud. |
| **Phase 6** | **Externalisation des Sections & Profilés** | Profilés métalliques et béton en JSON avec respect strict de la géométrie solide réelle OCCT (`BeamGeometry`). |
| **Phase 7** | **Externalisation des Câbles** | Câbles, torons et haubans en JSON selon l'architecture existante (`CableDefinition`). |
| **Phase 8** | **Versioning & Snapshots de Calcul** | Stockage des snapshots mécaniques dans le fichier `.tsa` pour garantir la reproductibilité des calculs. |
| **Phase 9** | **Optimisation : Lazy Loading & Cache** | Indexation rapide au démarrage, chargement à la demande des géométries lourdes. |
| **Phase 10** | **Interface Utilisateur Library Manager** | Fenêtre moderne "Gestionnaire de Bibliothèques" (catégories, recherche, rechargement en 1 clic, import/export). |
| **Phase 11** | **Packages .tsalib & Validation Globale** | Packaging installable et suite complète de tests de non-régression. |

---

## 7. Règle Fondamentale Respectée

```text
                  TSA.exe
                     │
              Extension System
                     │
        ┌────────────┴────────────┐
        │                         │
   Data Extensions          Code Extensions
        │                         │
     TSALib                     DLL
        │
┌───────┼────────┬─────────┐
│       │        │         │
Materials Sections Cables Textures
│       │        │         │
Profiles Rebar Prestress Standards

Flux de mise à jour :
Modifier JSON / Ajouter Texture → Clic "Recharger" → Immédiatement disponible dans TSA → AUCUNE RECOMPILATION
```
