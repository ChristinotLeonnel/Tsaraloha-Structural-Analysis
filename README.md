# TSA - Tsaraloha Structural Analysis

Logiciel de modélisation et d'analyse 3D orienté structure et génie civil, développé en **C++20** avec **Qt 6** et le noyau géométrique **OpenCASCADE (OCCT)**.

---

## 📋 Prérequis et Environnement

- **Système d'exploitation** : Windows 10 / 11 (x64)
- **Compilateur** : 
  - **MSVC** (Visual Studio 2022 / 2026 x64) avec support C++20 — *sélectionné automatiquement par défaut*
  - **MinGW-w64** (GCC 13+ / 16+ x64) — *sélectionné automatiquement si MSVC est absent, ou téléchargé/installé automatiquement si aucun compilateur n'est présent*
- **CMake** : Version 3.20 ou supérieure
- **Ninja** *(recommandé pour le développement)* : fourni avec Visual Studio (composant « Outils CMake pour Windows »), ou `winget install Ninja-build.Ninja`
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

### Option 2 : Presets Ninja (rapide — recommandé pour le développement)

Les presets `ninja-debug` et `ninja-release` utilisent **Ninja + MSVC**, avec compilation parallèle de tous les fichiers. Ils doivent être lancés depuis un terminal dont l'environnement Visual Studio est chargé (*x64 Native Tools Command Prompt for VS*, *Developer PowerShell for VS*, ou le script ci-dessous) :

```powershell
# 1. Charger l'environnement Visual Studio (une fois par fenêtre PowerShell)
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
& "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64

# 2. Configuration + compilation (Debug)
cmake --preset ninja-debug
cmake --build --preset ninja-debug

# Release
cmake --preset ninja-release
cmake --build --preset ninja-release
```

Le build est généré dans `build-ninja-debug/` ou `build-ninja-release/` (Ninja est mono-configuration : l'exécutable `TSA.exe` est directement à la racine de ce dossier, sans sous-dossier `Debug/` ou `Release/`).

> **Astuce :** ajoutez `-- -k 0` à la commande de build pour afficher toutes les erreurs d'un coup au lieu de s'arrêter à la première.

### Option 3 : Presets Visual Studio

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

### Option 4 : En ligne de commande standard

```powershell
# 1. Génération de la solution (OCCT et 3rdparty téléchargés automatiquement si absents)
cmake -B build -S . -DQt6_DIR="C:/Qt/6.11.2/msvc2022_64/lib/cmake/Qt6"

# 2. Compilation en Release
cmake --build build --config Release
```

> **Note :** À la fin de la compilation, le script CMake `cmake/DeployDependencies.cmake` et `windeployqt` déploient automatiquement toutes les DLL nécessaires (Qt, OCCT, 3rdparty) dans le dossier de sortie (`build/Release/` avec les générateurs Visual Studio, la racine de `build-ninja-*/` avec Ninja).

### Option 5 : Dans Visual Studio

Ouvrez simplement le dossier racine du projet dans Visual Studio. Les profils `CMakePresets.json` seront automatiquement détectés.

---

### ⚡ Temps de compilation

Le build est optimisé de la façon suivante :

- **`TSA_Core`** : la logique métier (modèle, coordonnées, grilles, commandes, IO, extensions…) est compilée **une seule fois** dans une bibliothèque `OBJECT` partagée par l'exécutable `TSA` et par la suite de tests `TSA_Tests` (auparavant, ces sources étaient compilées deux fois).
- **En-têtes précompilés (PCH)** pour Qt, les types de base OpenCASCADE et la STL, réutilisés par toutes les cibles.
- **Includes OpenCASCADE en `SYSTEM`** : analyse plus rapide et pas d'avertissements provenant d'OCCT.
- **Compilation parallèle** : `/MP` avec les générateurs Visual Studio ; Ninja parallélise nativement.

Options CMake utiles :

| Option | Défaut | Effet |
|---|---|---|
| `-DTSA_BUILD_TESTS=OFF` | `ON` | Ne compile pas la suite de tests |
| `-DTSA_USE_CCACHE=ON` | `OFF` | Utilise `ccache` avec Ninja (support limité des PCH avec MSVC : à tester avant adoption) |

Conseils complémentaires :

- Ajoutez le dossier du projet (et `build-ninja-*/`) aux **exclusions de Windows Defender** : l'analyse antivirus de chaque fichier objet ralentit fortement la compilation.
- Placez le projet sur un **SSD**, hors dossier synchronisé (OneDrive, Dropbox…).
- Si un fichier utilisant directement l'API Windows échoue avec `identificateur non déclaré` (ex. `MessageBoxA`), il doit être exclu du PCH : voir `SKIP_PRECOMPILE_HEADERS` dans `CMakeLists.txt`.

---

## 🚀 Lancement

### Méthode 1 : Script automatique (Recommandé)

Lancez simplement le fichier batch à la racine :
```cmd
run.bat
```
Ce script configure l'environnement d'exécution (variables `CSF_OCCTResourcePath`, `CSF_OCCTShadersPath` et `QT_PLUGIN_PATH`), ajoute les DLL au `PATH` et démarre l'application.

Sans argument, `run.bat` lance le `TSA.exe` **le plus récemment compilé** parmi les dossiers de build connus (`build-ninja-release`, `build-ninja-debug`, `build-debug\Debug`, `build\Release`, `build\Debug`, `build-msvc\*`). Pour choisir un build précis, passez son dossier en argument :

```cmd
run.bat build-ninja-debug
run.bat build\Release
```

### Méthode 2 : Lancement direct

Puisque les dépendances sont copiées automatiquement lors du build :
```powershell
.\build\Release\TSA.exe            # Presets Visual Studio
.\build-ninja-debug\TSA.exe        # Presets Ninja (Debug)
.\build-ninja-release\TSA.exe      # Presets Ninja (Release)
```

---

## ✨ Fonctionnalités Majeures

### 1. Modélisation Structurale & Noyau Géométrique 3D
- **Solides B-Rep Exacts (OpenCASCADE 8.0.1)** : Modélisation volumique réelle sans maillage polygonal grossier pour une précision géométrique et d'assemblage maximale.
- **Éléments Filaires 1D** : Poutres, poteaux, câbles/haubans élastiques tendus, barres spatiales génériques.
- **Générateur Automatique de Treillis** : Génération instantanée paramétrique de fermes métalliques de types *Warren* (diagonales alternées), *Pratt* (diagonales tendues) et *Howe* (diagonales comprimées).
- **Éléments Surfaciques 2D** : Dalles et planchers (report de charge 1 sens ou 2 sens), voiles banchés et murs porteurs, semelles de fondation.
- **Plans de Travail 3D Interactifs (WorkPlanes)** :
  - Définition arbitraire par 3 points, alignement automatique sur la normale de caméra ou sur les niveaux d'étages.
  - Projection bidirectionnelle non-destructive 2D $\leftrightarrow$ 3D et repères locaux d'éléments (LCS).
  - Moteur de magnétisme intelligent (accrochage extrémités, milieux, centres, grilles 3D).
- **Conditions d'Appuis 3D** :
  - Encastrements complets, rotules/articulations, appuis simples (rouleaux).
  - 6 degrés de liberté (DDL) paramétriques découplés ($T_x, T_y, T_z, R_x, R_y, R_z$).
  - Représentation graphique 3D B-Rep fidèle (plaques d'assise, cônes, cylindres de glissement).
- **Système d'Actions & Charges Eurocodes** :
  - Forces et moments nodaux ponctuels ($F_x, F_y, F_z, M_x, M_y, M_z$).
  - Charges linéiques réparties uniformes et trapézoïdales le long des barres.
  - Calcul et intégration automatique du poids propre des éléments.
  - Cas de charges et combinaisons d'actions aux états limites (ELU / ELS).

### 2. Moteur de Calcul OpenSees Intégré
- **Types d'Analyses** :
  - Statique linéaire : $[K]\{u\} = \{F\}$.
  - Statique non-linéaire (grands déplacements $P\text{-}\Delta$, non-linéarités géométriques).
  - Analyse modale dynamique (valeurs propres, pulsations $\omega$, fréquences $f$, périodes $T$ et modes de vibration).
  - Analyse Pushover / non-linéaire incrémentale.
- **Algorithmes de Résolution Non-Linéaires** : Newton-Raphson standard, Newton avec recherche linéaire (*NewtonLineSearch*), Newton modifié (*ModifiedNewton*), *Krylov-Newton*, *BFGS*, *Broyden*, *SecantNewton*.
- **Intégrateurs Numériques** : Contrôle de charge (*LoadControl*), Contrôle de déplacement (*DisplacementControl*), Longueur d'arc (*Arc-Length* / Crisfield), Norme de déplacement non équilibré minimale (*MinUnbalDispNorm*).
- **Formulations Barres & Treillis** : Éléments standards et corotatifs (*corotTruss*) pour la stabilité en grands déplacements.
- **Dialogue de Configuration Dédié (`AnalysisConfigDialog`)** : Paramétrage interactif et validation dynamique des combinaisons solveurs/algorithmes/intégrateurs.

### 3. Visualisation 3D des Résultats & Inspection
- **Déformée 3D Amplifiée** :
  - 3 modes d'affichage : Modèle non déformé seul (initial), Déformée seule, Superposition non déformé + déformé.
  - Normalisation et facteurs d'échelle automatiques calibrés sur l'envergure caractéristique ($L_{span}$ / Bounding Box), avec presets ($\times 1, \times 10, \times 100, \times 1000, \times 10000$) et facteur personnalisé.
- **4 Familles de Diagrammes 3D Orientés dans l'Espace Local** :
  - **Moments** : Flexion principale $M_z$, flexion secondaire $M_y$, torsion $M_x$.
  - **Efforts Tranchants** : Tranchant selon $Z$ local ($V_z$), tranchant selon $Y$ local ($V_y$).
  - **Effort Normal** : Traction / compression axiale $N$.
  - **Déplacements & Rotations** : Flèches transversales et axiales ($U_x, U_y, U_z, U_{res}$), rotations ($R_x, R_y, R_z$).
- **Légende 3D Interactive en Surimpression** : Titre de la sollicitation, extrema (min/max), facteur d'échelle effectif et unités physiques SI ($kN, kNm, mm, rad$).
- **Réactions d'Appuis 3D** : Flèches vectorielles proportionnelles avec affichage numérique des valeurs aux nœuds d'appuis.
- **Identification & Inspection des Nœuds** :
  - Détection automatique et mise en évidence visuelle des **nœuds libres** (non connectés à des barres) par des sphères magenta proéminentes.
  - Filtre d'affichage des nœuds : *Tous*, *Nœuds libres uniquement*, *Nœuds d'appuis uniquement*, *Nœuds sélectionnés uniquement*, *Masqués*.
  - Panneau d'inspection nodale dans l'Inspecteur des Propriétés : déplacements $U$, rotations $R$, réactions d'appui $F/M$ et statut de connectivité.
  - Cadrage caméra contextuel : *Cadrer Modèle*, *Cadrer Résultats*, *Cadrer Déformée*, *Cadrer Sélection*.
- **Navigation Multi-Incréments** : Curseur temporel et pas de charge ($\lambda$) pour inspecter pas à pas la convergence des calculs non-linéaires.

### 4. Espace Multi-Vues (Multiport)
- Découpage dynamique du viewport : Vue unique, Double horizontale (2H), Double verticale (2V), Grille $2 \times 2$ (4 vues indépendantes), Vue ongletisée.
- Rendu indépendant par port sans recalcul mécanique.

---

## 🎀 Ruban Principal & Barres d'Outils

L'interface utilisateur s'articule autour d'un **Ruban ergonomique moderne à 8 onglets thématiques** inspiré des standards CAO/BIM :

| Onglet | Panneaux & Outils Principaux |
|---|---|
| **1. Accueil** | **Projet** (Nouveau, Ouvrir, Enregistrer, Enregistrer sous), **Historique** (Annuler, Rétablir, Copier, Coller), **Accès Rapide** (Sélection, Poutre, Poteau, Dalle, Calcul Statique, Panneau Résultats 3D), **Vue 3D** (Vue 3D, Cadrer tout, Réinitialiser vue). |
| **2. Modélisation** | **Éléments Filaires (1D)** (Poutre, Poteau, Câble, Barre générique, Générateur de Treillis), **Éléments Surfaciques (2D)** (Dalle/Plancher, Voile/Mur, Semelle), **Nœuds & Primitives** (Placer Nœud, Cube), **Trame & Niveaux** (Créer Grille, Gestionnaire de Grilles, Gestionnaire d'Étages/Niveaux), **Préréglages**. |
| **3. Structure** | **Sections & Profilés** (Profilé I/H, Rectangulaire, Circulaire), **Matériaux** (Béton Armé EC2, Acier Structural EC3), **Conditions d'Appuis** (Encastrement, Articulation, Appui Simple), **Bibliothèques & TSALib** (Gestionnaire d'extensions TSALib, Catalogue de sections). |
| **4. Calcul** | **Actions & Charges** (Force Ponctuelle, Charge Répartie, Moment, Séisme EC8, Cas & Combinaisons), **Discrétisation** (Générer Maillage EF), **Solveur** (Calcul Statique, Analyse Modale, Pushover, Paramètres de Calcul & Solveurs OpenSees). |
| **5. Résultats** | **Panneau** (Ouvrir le panneau Résultats 3D), **Déformée 3D** (Activer/Masquer déformée), **Diagrammes 3D** ($M_z, M_y, M_x$, $V_z, V_y, N$, Flèches $U_z/U_{res}$, Masquer), **Réactions** (Afficher réactions 3D), **Cadrage** (Cadrer Déformée, Cadrer Résultats, Cadrer Modèle, Cadrer Tout), **Note de Calcul** (Inspecteur NDC), **Espace Multi-Vues** (1 vue, 2H, 2V, 2x2, Onglets). |
| **6. Édition** | **Sélection** (Mode sélection, Tout sélectionner), **Déplacement** (Déplacement 3D, Translation relative), **Copie & Duplication** (Copie 3D, Rotation 3D), **Repère** (Déplacer l'origine), **Presse-papier** (Copier, Coller), **Suppression** (Supprimer éléments sélectionnés). |
| **7. Affichage** | **Projections** (Vue 3D, Dessus, Dessous, Face, Arrière, Gauche, Droite, Isométrique, Accueil), **Navigation** (Zoom étendu, Zoom sélection, Zoom fenêtre, Zoom +/-, Historique vue précédente/suivante), **Plans & Coupes** (Vue normale au plan, Plans XY, XZ, YZ, Plan d'étage, Plan personnalisé, Coupes 3D), **Aides Visuelles** (Grille, Niveaux, Règles, Nœuds, Étiquettes, Magnétisme grille/objets, Plein écran), **Fenêtres & Docks** (Arbre du Modèle, Propriétés, Résultats 3D, Visibilité, Console). |
| **8. Outils** | **Inspection** (Mesurer distance spatiale 3D), **Environnement** (Basculer thème Sombre / Clair), **Documentation** (Aide intégrée, Guide des raccourcis clavier, À propos de TSA). |

---

## 🪟 Panneaux Docks Centralisés (`WindowManager`)

L'espace de travail est structuré par des panneaux ancrables et flottants orchestrés par le gestionnaire central `TSA::UI::WindowManager` avec profils de disposition mémorisés :

1. **Arbre du Modèle (`ModelTreeDock`)** : Hiérarchie complète des entités physiques (Nœuds, Poutres, Poteaux, Câbles, Dalles, Voiles, Niveaux, Grilles) avec synchronisation bidirectionnelle de sélection.
2. **Inspecteur des Propriétés (`PropertiesDock`)** : Édition contextuelle des sections, matériaux, dimensions, excentrements, et volet d'inspection nodale des résultats de calcul OpenSees.
3. **Panneau de Contrôle des Résultats 3D (`ResultsDockWidget`)** :
   - Sélection instantanée des grandeurs affichées (déformée, diagrammes 3D $N, V, M, U, R$).
   - Navigation interactive dans les incréments non-linéaires (slider, spinbox et facteur $\lambda$).
   - Sélecteur de mode de déformée (initial, déformé, superposition) et presets d'amplification.
   - Filtrage dynamique des nœuds (tous, libres, appuis).
   - Boutons de cadrage contextuels.
4. **Calques & Visibilité (`VisibilityDock`)** : Contrôle indépendant de l'affichage des grilles, niveaux, cotes, étiquettes, charges, repères et plans de travail.
5. **Éléments Structuraux (`StructuralElementsDock`)** : Palette d'accès rapide pour le tracé interactif des éléments 1D et 2D.
6. **Plans de Travail & Caméra (`ProjectionViewDock`)** : Gestionnaire des plans de travail multiples, modes de projection et alignement géométrique.
7. **Console de Diagnostics & CLI (`LogConsoleDock`)** : Historique d'exécution temps réel, diagnostic des solveurs et ligne de commande rapide.

---

## 🧪 Tests Unitaires

Le projet inclut une suite complète de **84 bancs d'essais automatisés** (100% de réussite) validant l'ensemble de la chaîne de calcul et de modélisation :

```powershell
# Compilation de la cible de tests (presets Ninja)
cmake --build --preset ninja-debug --target TSA_Tests

# Exécution directe de la suite de tests (84 / 84 PASS)
.\build-ninja-debug\TSA_TestSuite.exe

# Ou via CTest
ctest --test-dir build-ninja-debug --output-on-failure
```

Avec les générateurs Visual Studio :

```powershell
cmake --build build --config Release --target TSA_Tests
.\build\Release\TSA_TestSuite.exe
```

### Couverture des 14 Suites de Tests :
- **Suites 1–8** : Modélisation 3D solide B-Rep OCCT, système de câbles/haubans, format binaire `.tsa`, Undo/Redo transactionnel.
- **Suite 9** : Système d'extensions **TSALib** (registre, chargement différé, cache multi-niveaux, packaging `.tsalib`).
- **Suite 10** : Plans de travail 3D interactifs (WorkPlanes), transformations de coordonnées, magnétisme spatial.
- **Suite 11** : Gestionnaire centralisé de fenêtres et profils de disposition (`WindowManager`).
- **Suite 12** : Gestionnaire de nœuds centralisé, sélection et accrochage non-destructif.
- **Suite 13** : Système de charges structurales, poids propre automatique, cas et combinaisons Eurocodes, génération de scripts OpenSees.
- **Suite 14** : Solveur OpenSees, immutabilité des snapshots de calcul, conditions d'appuis 3D (6 DDL), calculs multi-pas, 4 familles de diagrammes 3D, détection des nœuds libres et synchronisation modèle/résultats.

---

## 📦 Système d'Extensions TSALib

TSA intègre le système d'extensions et de bibliothèques d'ingénierie **TSALib** (`TSA::ExtensionSystem`) :
- **100% Découplé** : Matériaux Eurocodes, profilés métalliques, sections, câbles, haubans et textures PBR stockés en fichiers JSON et PNG ouverts sans recompilation.
- **Gestionnaire Graphique** : Accessible via l'onglet *Structure* > *Gestionnaire TSALib...* (Recherche instantanée, fiches techniques HTML, validation globale).
- **Rechargement à Chaud (Hot Reload)** : Actualisation immédiate du modèle 3D et des listes en 1 clic.
- **Packaging Autonome (.tsalib)** : Importation et exportation de bibliothèques complètes signées SHA-256 avec protection anti-Path-Traversal.
- **Reproductibilité des Calculs** : Snapshots mécaniques scellés dans le fichier `.tsa` (`CHUNK_SNAP`).

> 📖 **Consultez la documentation détaillée : [docs/TSALIB_SYSTEM.md](docs/TSALIB_SYSTEM.md).**

---

## 🎮 Navigation & Raccourcis 3D

| Action | Contrôle |
|---|---|
| **Rotation / Orbite 3D** | Clic droit maintenu + Déplacement |
| **Panoramique (Pan)** | Clic molette maintenu + Déplacement |
| **Zoom avant / arrière** | Molette de la souris |
| **Centrer la vue (Fit All)** | Touche <kbd>F</kbd> |
| **Cadrer la sélection** | Touches <kbd>Maj</kbd> + <kbd>F</kbd> |
| **Vue isométrique initiale** | Touche <kbd>R</kbd> |
| **Vue d'accueil (Home)** | Touche <kbd>Origine (Home)</kbd> |
| **Vue de dessus (Plan XY)** | Pavé numérique <kbd>7</kbd> |
| **Vue de face (Plan XZ)** | Pavé numérique <kbd>1</kbd> |
| **Vue de droite (Plan YZ)** | Pavé numérique <kbd>3</kbd> |
| **Inspecteur des Propriétés** | Touche <kbd>P</kbd> |
| **Lancer le Calcul EF** | Touche <kbd>F5</kbd> |
| **Générateur Note de Calcul** | Touche <kbd>F8</kbd> |
| **Plein Écran** | Touche <kbd>F11</kbd> |
| **Aide contextuelle** | Touche <kbd>F1</kbd> |

---

## 📁 Architecture du Projet

```text
TSA/
├── cmake/                      # Scripts CMake et déploiement automatique OCCT/DLLs
├── docs/                       # Guides et spécifications techniques détaillées
│   ├── ARCHITECTURE.md         # Architecture générale et flux de données
│   ├── TSALIB_SYSTEM.md        # Guide complet du système d'extensions TSALib
│   ├── TSA_FILE_FORMAT.md      # Spécification du format binaire .tsa (chunks)
│   ├── TSA_DIAGNOSTICS.md      # Documentation du système de logs et télémétrie
│   ├── UI.md                   # Architecture de l'interface utilisateur Qt 6
│   └── MODEL.md                # Spécification du modèle structural de données
├── Extensions/                 # Extensions et bibliothèques de calcul installées
│   └── TSALib/                 # Bibliothèque standard Eurocodes (Matériaux, Sections, Câbles, Textures)
├── scripts/                    # Scripts PowerShell d'orchestration et détection compilateurs
├── opencascade-8.0.1-vc14-64/   # SDK OpenCASCADE local (téléchargé si absent)
├── 3rdparty-vc14-64/           # Bibliothèques tierces (téléchargées si absentes)
├── resources/                  # Icônes et fichiers de ressources Qt (.qrc)
├── src/
│   ├── Analysis/               # Solveurs EF (OpenSees, analyse modale, pushover, résultats)
│   ├── App/                    # Classe d'application principale et bootstrap
│   ├── Commands/               # Commandes CAO réversibles (ICommand)
│   ├── Coordinate/             # Gestion des points 3D, niveaux et plans de travail
│   ├── Diagnostics/            # Moteur de logging, télémétrie et rapports de crash
│   ├── ExtensionSystem/        # Moteur TSALib (Registry, Loader, Validator, Cache, Packager)
│   ├── Geometry/               # Géométries solides B-Rep OCCT et diagrammes 3D
│   ├── Grid/                   # Définition, rendu et magnétisme des grilles 3D
│   ├── IO/                     # Format binaire .tsa, chunks, snapshots et prévisualisations
│   ├── Model/                  # Modèle structurel (nœuds, barres, câbles, dalles, appuis, charges)
│   ├── NDC/                    # Moteur de génération des Notes de Calcul réglementaires
│   ├── UI/                     # Interface Qt 6 (Ruban 8 onglets, Docks, Dialogues, Profils)
│   ├── UndoRedo/               # Piles de snapshots et gestionnaire Undo/Redo
│   └── Viewer/                 # Vue 3D OpenCASCADE (OccView, textures PBR, sélection, résultats 3D)
├── tests/                      # Suite de tests unitaires automatisés (84 bancs d'essais)
├── AGENTS.md                   # Directives fondamentales pour agents IA et règles de build
├── CMakeLists.txt              # Configuration principale CMake (cibles TSA_Core, TSA, TSA_Tests)
├── CMakePresets.json           # Presets Visual Studio et Ninja (ninja-debug, ninja-release)
└── run.bat                     # Script de lancement rapide automatique
```