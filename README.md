# TSA - Tsaraloha Structural Analysis

Logiciel de modélisation et d'analyse 3D orienté structure et génie civil, développé en **C++20** avec **Qt 6** et le noyau géométrique **OpenCASCADE (OCCT)**.

---

## 📋 Prérequis et Environnement

- **Système d'exploitation** : Windows 10 / 11 (x64)
- **Compilateur** : MSVC (Visual Studio 2022 / 2026 x64) avec support complet C++20
- **CMake** : Version 3.20 ou supérieure
- **Qt 6** : Qt 6.2+ (`Core`, `Gui`, `Widgets`, `Svg`) — *ex. `C:\Qt\6.11.2\msvc2022_64`*
- **OpenCASCADE** : OCCT 8.0.1 (**téléchargé et extrait automatiquement par CMake** si absent)
- **Dépendances tierces (3rdparty)** : FreeType, TBB, FreeImage, Jemalloc, etc. (**téléchargées automatiquement par CMake** si absentes)

---

## 🛠️ Configuration & Compilation

> **Téléchargement automatique des bibliothèques :** Si `opencascade-8.0.1-vc14-64` ou `3rdparty-vc14-64` ne sont pas encore présents dans votre copie locale (ex: clone Git frais), CMake les télécharge et les décompresse automatiquement depuis les dépôts officiels GitHub dès la première configuration. Vous pouvez désactiver cette option via `-DTSA_AUTO_DOWNLOAD_DEPS=OFF`.

### Option 1 : Avec CMake Presets (Recommandé)

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

### Option 2 : En ligne de commande standard

```powershell
# 1. Génération de la solution (ajuster le chemin Qt si nécessaire, OCCT sera trouvé ou téléchargé automatiquement)
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
├── cmake/                      # Scripts d'automatisation de déploiement
├── opencascade-8.0.1-vc14-64/   # SDK OpenCASCADE local
├── 3rdparty-vc14-64/           # Bibliothèques tierces (FreeType, TBB...)
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