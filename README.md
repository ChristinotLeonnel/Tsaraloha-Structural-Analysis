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

Le projet inclut une suite de tests automatisés (QtTest) validant les systèmes de coordonnées, les niveaux et la sérialisation JSON :

```powershell
# Compilation de la cible de tests
cmake --build build --config Release --target TSA_Tests

# Exécution des tests via CTest
ctest --test-dir build -C Release --output-on-failure
```

Ou directement :
```powershell
.\build\Release\TSA_Tests.exe
```

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
├── scripts/                    # Scripts PowerShell d'orchestration et détection compilateurs
│   ├── setup_build.ps1         # Script principal (détection, vérification, build)
│   ├── detect_compiler.ps1     # Détection rigoureuse MSVC et MinGW
│   ├── install_mingw.ps1       # Téléchargement sécurisé WinLibs MinGW-w64
│   ├── detect_qt.ps1           # Détection Qt et vérification de toolchain
│   └── detect_dependencies.ps1 # Détection de compatibilité OCCT et VTK
├── opencascade-8.0.1-vc14-64/   # SDK OpenCASCADE local (téléchargé si absent)
├── 3rdparty-vc14-64/           # Bibliothèques tierces (téléchargées si absentes)
├── resources/                  # Icônes et fichiers de ressources Qt (.qrc)
├── src/
│   ├── App/                    # Classe d'application principale
│   ├── Coordinate/             # Gestion des points 3D, niveaux et repères
│   ├── Geometry/               # Utilitaires géométriques OCCT
│   ├── Grid/                   # Définition et rendu des grilles 3D
│   ├── Model/                  # Modèle structurel (nœuds, barres, charges)
│   ├── UI/                     # Interface utilisateur (MainWindow, dialogues, widgets)
│   └── Viewer/                 # Vue 3D OpenCASCADE intégrée dans Qt
├── tests/                      # Tests unitaires automatisés
├── CMakeLists.txt              # Configuration principale CMake
├── CMakePresets.json           # Presets de configuration et build
└── run.bat                     # Script de lancement rapide
```