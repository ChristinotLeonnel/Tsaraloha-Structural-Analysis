# TSA - Tsaraloha Structural Analysis

Logiciel de modélisation et d'analyse 3D orienté structure et génie civil, développé en **C++20** avec **Qt 6** et le noyau géométrique **OpenCASCADE (OCCT)**.

---

## 📋 Prérequis et Environnement

- **Système d'exploitation** : Windows 10 / 11 (x64)
- **Compilateur** : 
  - **MSVC** (Visual Studio 2022 / 2026 x64) avec support C++20 — *sélectionné automatiquement par défaut*
  - **MinGW-w64** (GCC 13+ / 16+ x64) — *sélectionné automatiquement si MSVC est absent, ou téléchargé/installé automatiquement si aucun compilateur n'est présent*
- **CMake** : Version 3.20 ou supérieure
- **Qt 6** : Qt 6.2+ (`Core`, `Gui`, `Widgets`, `Svg`) — *ex. `C:\Qt\6.11.2\msvc2022_64` ou `C:\Qt\6.11.2\mingw_64`*
- **OpenCASCADE** : OCCT 8.0.1 (**téléchargé et extrait automatiquement par CMake** si absent)
- **Dépendances tierces (3rdparty)** : FreeType, TBB, FreeImage, Jemalloc, etc. (**téléchargées automatiquement par CMake** si absentes)

---

## 🛠️ Configuration & Compilation

### Option 1 : Script Intelligent Tout-en-Un (Fortement Recommandé)

Le projet intègre un système d'orchestration PowerShell qui :
1. Détecte automatiquement si **Visual Studio / MSVC** est installé.
2. Si MSVC est absent, détecte **MinGW-w64** existant.
3. Si aucun compilateur n'est installé, télécharge et installe automatiquement **MinGW-w64 x64** officiel (WinLibs) avec contrôle d'intégrité SHA256.
4. Vérifie la stricte compatibilité de toolchain avec votre installation **Qt 6** et **OCCT**.
5. Configure CMake dans un dossier dédié non-destructif (`build-msvc/` ou `build-mingw/`).

```powershell
# Détection automatique du compilateur et configuration CMake
powershell -ExecutionPolicy Bypass -File .\scripts\setup_build.ps1

# Configuration ET compilation immédiate en Release
powershell -ExecutionPolicy Bypass -File .\scripts\setup_build.ps1 -Build

# Mode Debug complet
powershell -ExecutionPolicy Bypass -File .\scripts\setup_build.ps1 -Config Debug -Build
```

### Option 2 : Avec CMake Presets

```powershell
# Configuration (Release)
cmake --preset windows-x64-release

# Compilation du projet
cmake --build --preset windows-x64-release
```

Pour le mode Debug :
```powershell
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
```

### Option 3 : En ligne de commande standard

```powershell
# 1. Génération de la solution (OCCT et 3rdparty téléchargés automatiquement si absents)
cmake -B build -S . -DQt6_DIR="C:/Qt/6.11.2/msvc2022_64/lib/cmake/Qt6"

# 2. Compilation en Release
cmake --build build --config Release
```

> **Note :** À la fin de la compilation, le script CMake `cmake/DeployDependencies.cmake` et `windeployqt` déploient automatiquement toutes les DLL nécessaires (Qt, OCCT, 3rdparty) dans le dossier de sortie (`build/Release/`).

### Option 3 : Dans Visual Studio

Ouvrez simplement le dossier racine du projet dans Visual Studio. Les profils `CMakePresets.json` seront automatiquement détectés.

---

## 🚀 Lancement

### Méthode 1 : Script automatique (Recommandé)

Lancez simplement le fichier batch à la racine :
```cmd
run.bat
```
Ce script configure l'environnement d'exécution (variables `CSF_OCCTResourcePath`, `CSF_OCCTShadersPath` et `QT_PLUGIN_PATH`), ajoute les DLL au `PATH` et démarre l'application.

### Méthode 2 : Lancement direct

Puisque les dépendances sont copiées automatiquement lors du build :
```powershell
.\build\Release\TSA.exe
```

---

## 🧪 Tests Unitaires

Le projet inclut une suite complète de **48 bancs d'essais automatisés** validant rigoureusement la modélisation 3D, le système de câbles/haubans, le diagnostic temps réel et l'ensemble du système d'extensions **TSALib** :

```powershell
# Compilation de la cible de tests
cmake --build build --config Release --target TSA_Tests

# Exécution directe de la suite de tests (48 / 48 PASS)
.\build\Release\TSA_TestSuite.exe
```

---

## 📦 Système d'Extensions TSALib

TSA intègre le système d'extensions et de bibliothèques d'ingénierie **TSALib** (`TSA::ExtensionSystem`) :
- **100% Découplé** : Matériaux Eurocodes, profilés métalliques, sections, câbles, haubans et textures PBR stockés en fichiers JSON et PNG ouverts sans recompilation.
- **Gestionnaire Graphique** : Accessible via l'onglet *Structure & Sections* > *Gestionnaire TSALib...* (Recherche instantanée, fiches techniques HTML, validation globale).
- **Rechargement à Chaud (Hot Reload)** : Actualisation immédiate du modèle 3D et des listes en 1 clic.
- **Packaging Autonome (.tsalib)** : Importation et exportation de bibliothèques complètes signées SHA-256 avec protection anti-Path-Traversal.
- **Reproductibilité des Calculs** : Snapshots mécaniques scellés dans le fichier `.tsa` (`CHUNK_SNAP`).

> 📖 **Consultez la documentation détaillée : [docs/TSALIB_SYSTEM.md](file:///e:/Book/Dev/TSA/docs/TSALIB_SYSTEM.md).**

---

## 🎮 Navigation & Raccourcis 3D

| Action | Contrôle |
|---|---|
| **Rotation / Orbite 3D** | Clic droit maintenu + Déplacement |
| **Panoramique (Pan)** | Clic molette maintenu + Déplacement |
| **Zoom avant / arrière** | Molette de la souris |
| **Centrer la vue (Fit All)** | Touche <kbd>F</kbd> |
| **Vue isométrique initiale** | Touche <kbd>R</kbd> |
| **Grille de repère** | Bouton barre d'outils / Dialogue de grille |

---

## 📁 Architecture du Projet

```text
TSA/
├── cmake/                      # Scripts CMake et téléchargement auto OCCT
├── docs/                       # Guides et spécifications techniques détaillées
│   ├── TSALIB_SYSTEM.md        # Guide complet du système d'extensions TSALib
│   ├── TSA_FILE_FORMAT.md      # Spécification du format binaire .tsa (chunks)
│   └── TSA_DIAGNOSTICS.md      # Documentation du système de logs et télémétrie
├── Extensions/                 # Extensions et bibliothèques de calcul installées
│   └── TSALib/                 # Bibliothèque standard Eurocodes (Matériaux, Sections, Câbles, Textures)
├── scripts/                    # Scripts PowerShell d'orchestration et détection compilateurs
├── opencascade-8.0.1-vc14-64/   # SDK OpenCASCADE local (téléchargé si absent)
├── 3rdparty-vc14-64/           # Bibliothèques tierces (téléchargées si absentes)
├── resources/                  # Icônes et fichiers de ressources Qt (.qrc)
├── src/
│   ├── App/                    # Classe d'application principale
│   ├── Coordinate/             # Gestion des points 3D, niveaux et repères
│   ├── Diagnostics/            # Moteur de logging, télémétrie et rapports de crash
│   ├── ExtensionSystem/        # Moteur TSALib (Registry, Loader, Validator, Cache, Packager)
│   ├── Geometry/               # Utilitaires géométriques et solides B-Rep OCCT
│   ├── Grid/                   # Définition, rendu et magnétisme des grilles 3D
│   ├── IO/                     # Format binaire .tsa, chunks, snapshots et prévisualisations
│   ├── Library/                # Catalogues et gestionnaires de sections/matériaux
│   ├── Model/                  # Modèle structurel (nœuds, barres, câbles, dalles, charges)
│   ├── UI/                     # Interface utilisateur Qt 6 (Ruban, docks, dialogues, widgets)
│   └── Viewer/                 # Vue 3D OpenCASCADE (OccView, textures PBR, sélection)
├── tests/                      # Suite de tests unitaires automatisés (48 bancs d'essais)
├── CMakeLists.txt              # Configuration principale CMake
├── CMakePresets.json           # Presets de configuration et build
└── run.bat                     # Script de lancement rapide
```