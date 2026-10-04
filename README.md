<p align="center">
  <img src="resources/branding/TSA_Banner.svg" alt="TSA - Tsaraloha Structural Analysis Banner" width="100%" />
</p>

<p align="center">
  <img src="resources/branding/TSA_Logo.svg" alt="TSA Logo" width="130" />
</p>

<h1 align="center">TSA — Tsaraloha Structural Analysis</h1>

<p align="center">
  <strong>Plateforme logicielle de modélisation CAO 3D B-Rep exacte et d'analyse structurelle par éléments finis (FEA) pour le génie civil.</strong><br>
  Développé en <b>C++20</b> avec <b>Qt 6</b>, le noyau géométrique <b>OpenCASCADE (OCCT)</b> et le moteur de calcul <b>OpenSees</b>.
</p>

<p align="center">
  <a href="https://en.cppreference.com/w/cpp/20"><img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++20" /></a>
  <a href="https://www.qt.io/"><img src="https://img.shields.io/badge/Qt-6.2%2B-41CD52?style=for-the-badge&logo=qt&logoColor=white" alt="Qt 6" /></a>
  <a href="https://dev.opencascade.org/"><img src="https://img.shields.io/badge/OpenCASCADE-8.0.1-0072FF?style=for-the-badge" alt="OpenCASCADE 8.0.1" /></a>
  <a href="https://opensees.berkeley.edu/"><img src="https://img.shields.io/badge/OpenSees-FEA%20Engine-E91E63?style=for-the-badge" alt="OpenSees" /></a>
  <a href="https://www.microsoft.com/windows"><img src="https://img.shields.io/badge/Platform-Windows%20x64-0078D6?style=for-the-badge&logo=windows&logoColor=white" alt="Windows x64" /></a>
  <a href="https://cmake.org"><img src="https://img.shields.io/badge/CMake-Ninja-064F8C?style=for-the-badge&logo=cmake&logoColor=white" alt="CMake & Ninja" /></a>
  <a href="tests/"><img src="https://img.shields.io/badge/Tests-84%2F84%20Passing-10b981?style=for-the-badge&logo=checkmarx&logoColor=white" alt="Tests 84/84 Passing" /></a>
  <a href="docs/NORMATIVE_SYSTEM.md"><img src="https://img.shields.io/badge/Eurocodes-EN%201990--1999-f59e0b?style=for-the-badge" alt="Eurocodes" /></a>
</p>

<p align="center">
  <a href="#-getting-started--démarrage-rapide">Démarrage Rapide</a> •
  <a href="#-configuration--compilation">Compilation</a> •
  <a href="#-fonctionnalités-majeures">Fonctionnalités</a> •
  <a href="#-ruban-principal--ergonomie-cao">Interface</a> •
  <a href="#-architecture-logicielle--source-de-vérité">Architecture</a> •
  <a href="#-tests-automatisés">Tests</a> •
  <a href="#-système-dextensions-tsalib">TSALib</a> •
  <a href="#-documentation-technique">Documentation</a>
</p>

---

## 📖 Présentation

**TSA (Tsaraloha Structural Analysis)** est une suite logicielle desktop d'ingénierie des structures de nouvelle génération. Elle fusionne la fidélité géométrique volumique B-Rep d'un modeleur CAO industriel (**OpenCASCADE Technology**) avec la puissance non-linéaire d'un moteur de calcul aux éléments finis de référence (**OpenSees**), le tout orchestré par une interface utilisateur réactive et moderne (**Qt 6**) organisée autour d'un Ruban ergonomique à 8 onglets et de fenêtres ancrables intelligentes.

TSA permet de concevoir, modéliser, charger, analyser et inspecter en 3D interactive des bâtiments à étages, des halles industrielles, des fermes et treillis spatiaux, des structures haubanées et des systèmes de fondations selon les normes européennes (**Eurocodes**).

---

## 🚀 Getting Started / Démarrage Rapide

### Prérequis Système & Environnement
* **Système d'exploitation :** Windows 10 / 11 (64-bit)
* **Compilateur :** MSVC (Visual Studio 2022 ou 2026 x64) avec support complet C++20 *(recommandé)* ou MinGW-w64 (GCC 13+)
* **CMake :** Version 3.20+
* **Ninja :** Intégré à Visual Studio (composant *Outils CMake pour Windows*) ou via `winget install Ninja-build.Ninja`
* **Qt 6 :** Version 6.2+ (`Core`, `Gui`, `Widgets`, `Svg`) — ex. `C:\Qt\6.11.2\msvc2022_64`
* **OpenCASCADE 8.0.1 & Dépendances 3rdparty :** Téléchargées et extraites automatiquement par CMake si absentes de la machine.

---

### <ins>1. Téléchargement du Dépôt</ins>

Clonez le projet avec Git :

```powershell
git clone https://github.com/ChristinotLeonnel/Tsaraloha-Structural-Analysis.git TSA
cd TSA
```

---

### <ins>2. Configuration Automatique des Dépendances</ins>

Le projet intègre un orchestrateur intelligent en PowerShell (`scripts/setup_build.ps1`) qui inspecte votre environnement, configure les toolchains, télécharge les SDKs nécessaires et prépare CMake sans aucune action manuelle complexe :

```powershell
# Détection automatique de la toolchain et configuration du build
powershell -ExecutionPolicy Bypass -File .\scripts\setup_build.ps1
```

---

### <ins>3. Compilation du Projet</ins>

#### Option A : Ninja Presets *(Recommandé pour le développement quotidien)*
Ninja offre la vitesse de compilation maximale grâce à la compilation parallèle et aux en-têtes précompilés (PCH) :

```powershell
# 1. Charger l'environnement MSVC dans le terminal courant (une seule fois par session PowerShell)
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
& "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64

# 2. Configuration & compilation Debug
cmake --preset ninja-debug
cmake --build --preset ninja-debug -- -k 0

# Ou compilation Release
cmake --preset ninja-release
cmake --build --preset ninja-release
```

#### Option B : Visual Studio Presets
```powershell
# Configuration & compilation Release
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
```

#### Option C : Script Tout-en-Un
```powershell
# Configure et compile automatiquement en mode Release
powershell -ExecutionPolicy Bypass -File .\scripts\setup_build.ps1 -Build
```

---

### <ins>4. Lancement de TSA</ins>

Utilisez le script automatique à la racine du projet :

```cmd
run.bat
```

> **Note :** `run.bat` initialise les variables d'environnement OpenCASCADE (`CSF_OCCTResourcePath`, `CSF_OCCTShadersPath`), injecte les bibliothèques dynamiques Qt et tierces dans le `PATH`, et démarre automatiquement le binaire le plus récent (`build-ninja-debug`, `build-ninja-release` ou `build/Release`). Pour cibler un dossier spécifique :
> ```cmd
> run.bat build-ninja-debug
> ```

---

## ⚡ Optimisation du Pipeline de Build

Le temps de compilation de TSA est optimisé pour les développeurs C++ exigeants :

* **Bibliothèque `TSA_Core` (OBJECT) :** Tout le code métier (modèle de données, grilles, calculs, commandes, extensions, I/O) est compilé **une seule et unique fois**, puis réutilisé directement par l'exécutable principal `TSA` et par la suite de tests `TSA_Tests`.
* **En-têtes Précompilés (PCH) Partagés :** Les en-têtes volumineux stables (Qt Widgets/Core, OpenCASCADE `gp_*`/`TopoDS_Shape`, STL C++20) sont précompilés et partagés entre toutes les cibles.
* **Includes OpenCASCADE en `SYSTEM` :** Élimine le coût d'analyse des avertissements issus des bibliothèques externes.

---

## ✨ Fonctionnalités Majeures

### 1. 🏗️ Modélisation Structurale & Noyau CAO 3D B-Rep
* **Solides B-Rep Exacts (OpenCASCADE 8.0.1)** : Pas de facettisation polygonale approximative ; géométrie volumique exacte pour tous les profilés, nœuds et assemblages.
* **Éléments Filaires 1D** : Poutres, poteaux, barres spatiales génériques, câbles et haubans tendus avec prise en compte du comportement élastique non-linéaire.
* **Générateur Automatique de Treillis** : Création instantanée et paramétrique de fermes métalliques de types **Warren** (diagonales alternées), **Pratt** (diagonales tendues) et **Howe** (diagonales comprimées).
* **Éléments Surfaciques 2D** : Dalles et planchers (report de charge unidirectionnel ou bidirectionnel), voiles et murs porteurs banchés, semelles superficielles de fondation.
* **Plans de Travail 3D Interactifs (WorkPlanes)** : Définition par 3 points arbitraires ou alignement sur les niveaux d'étages ; projection bidirectionnelle non-destructive 2D $\leftrightarrow$ 3D et magnétisme intelligent (extrémités, milieux, centres, grilles).
* **Conditions d'Appuis Spatiales (6 DDL)** : Encastrements complets, rotules/articulations, appuis simples (rouleaux), degrés de liberté découplés ($T_x, T_y, T_z, R_x, R_y, R_z$) représentés par de véritables solides B-Rep (plaques d'assise, cônes, cylindres).
* **Système d'Actions & Combinaisons Eurocodes** : Forces et moments nodaux, charges linéiques réparties uniformes et trapézoïdales, calcul automatique du poids propre volumique, cas de charges et combinaisons ELU / ELS.

### 2. 🧮 Moteur de Calcul OpenSees Intégré
* **Analyses Disponibles** :
  * Statique linéaire : $[K]\{u\} = \{F\}$.
  * Statique non-linéaire au second ordre (effets $P\text{-}\Delta$, non-linéarités géométriques).
  * Analyse modale dynamique (pulsations $\omega$, fréquences propres $f$, périodes fondamentales $T$, vecteurs de modes propres).
  * Analyse Pushover (poussée progressive non-linéaire incrémentale).
* **Algorithmes de Résolution Numérique** : Newton-Raphson standard, Newton avec recherche linéaire (*NewtonLineSearch*), Newton modifié, *Krylov-Newton*, *BFGS*, *Broyden*, *SecantNewton*.
* **Intégrateurs de Charge & Déplacement** : *LoadControl*, *DisplacementControl*, Longueur d'arc (*Arc-Length* / algorithme de Crisfield), Norme de déplacement non équilibré minimale (*MinUnbalDispNorm*).
* **Formulations Avancées** : Éléments standards et corotatifs (*corotTruss*) pour la stabilité en grands déplacements.

### 3. 📊 Visualisation 3D des Résultats & Inspection
* **Déformée 3D Amplifiée** : 3 modes d'affichage (Initial non déformé, Déformé seul, Superposition des deux) avec calibrage automatique sur l'envergure caractéristique du modèle ($L_{span}$).
* **4 Familles de Diagrammes 3D Orientés dans les Repères Locaux** :
  * **Moments de flexion & torsion :** $M_z$ (axe fort), $M_y$ (axe faible), $M_x$ (torsion).
  * **Efforts tranchants :** $V_z$ et $V_y$.
  * **Effort normal :** Traction et compression axiale $N$.
  * **Déplacements et rotations :** $U_x, U_y, U_z, U_{res}$, rotations nodales $R_x, R_y, R_z$.
* **Légende 3D Interactive en Surimpression** : Titre de l'effort, valeurs extrêmes (min/max), échelle effective et unités physiques du Système International ($kN, kNm, mm, rad$).
* **Réactions d'Appuis Vectorielles** : Flèches 3D proportionnelles avec affichage dynamique des valeurs aux nœuds d'appuis.
* **Détection Automatique des Nœuds Libres** : Signalement visuel proéminent des nœuds orphelins (sphères magenta) et filtrage contextuel (*Tous*, *Libres*, *Appuis*, *Sélectionnés*).
* **Navigation Multi-Incréments** : Slider interactif pour inspecter le modèle étape par étape lors des calculs non-linéaires pas à pas ($\lambda$).

### 4. 🖥️ Espace Multi-Vues (Multiport)
* Découpage dynamique de la fenêtre 3D : Vue unique, Double vue horizontale (2H), Double vue verticale (2V), Grille $2 \times 2$ (4 vues simultanées indépendantes), Vue ongletisée.
* Rendu graphique indépendant par port sans recalcul mécanique.

---

## 🎀 Ruban Principal & Ergonomie CAO

L'interface graphique est conçue autour d'un **Ruban moderne à 8 onglets contextuels** inspiré des standards CAO/BIM :

| Onglet | Panneaux & Outils Principaux |
| :--- | :--- |
| **1. Accueil** | **Projet** (Nouveau, Ouvrir, Enregistrer, Enregistrer sous), **Historique** (Annuler, Rétablir, Copier, Coller), **Accès Rapide** (Sélection, Poutre, Poteau, Dalle, Calcul Statique, Panneau Résultats 3D), **Vue 3D** (Vue 3D, Cadrer tout, Réinitialiser vue). |
| **2. Modélisation** | **Éléments Filaires (1D)** (Poutre, Poteau, Câble, Barre générique, Treillis automatique), **Éléments Surfaciques (2D)** (Dalle/Plancher, Voile/Mur, Semelle), **Nœuds & Primitives** (Placer Nœud, Cube), **Trame & Niveaux** (Créer Grille, Gestionnaire de Grilles, Gestionnaire d'Étages/Niveaux), **Préréglages**. |
| **3. Structure** | **Sections & Profilés** (Profilé I/H, Rectangulaire, Circulaire), **Matériaux** (Béton Armé EC2, Acier Structural EC3), **Conditions d'Appuis** (Encastrement, Articulation, Appui Simple), **Bibliothèques & TSALib** (Gestionnaire d'extensions TSALib, Catalogue de sections). |
| **4. Calcul** | **Actions & Charges** (Force Ponctuelle, Charge Répartie, Moment, Séisme EC8, Cas & Combinaisons), **Discrétisation** (Générer Maillage EF), **Solveur** (Calcul Statique, Analyse Modale, Pushover, Paramètres de Calcul & Solveurs OpenSees). |
| **5. Résultats** | **Panneau** (Ouvrir le panneau Résultats 3D), **Déformée 3D** (Activer/Masquer déformée), **Diagrammes 3D** ($M_z, M_y, M_x$, $V_z, V_y, N$, Flèches $U_z/U_{res}$, Masquer), **Réactions** (Afficher réactions 3D), **Cadrage** (Cadrer Déformée, Cadrer Résultats, Cadrer Modèle, Cadrer Tout), **Note de Calcul** (Inspecteur NDC), **Espace Multi-Vues** (1 vue, 2H, 2V, 2x2, Onglets). |
| **6. Édition** | **Sélection** (Mode sélection, Tout sélectionner), **Déplacement** (Déplacement 3D, Translation relative), **Copie & Duplication** (Copie 3D, Rotation 3D), **Repère** (Déplacer l'origine), **Presse-papier** (Copier, Coller), **Suppression** (Supprimer éléments sélectionnés). |
| **7. Affichage** | **Projections** (Vue 3D, Dessus, Dessous, Face, Arrière, Gauche, Droite, Isométrique, Accueil), **Navigation** (Zoom étendu, Zoom sélection, Zoom fenêtre, Zoom +/-, Historique vue précédente/suivante), **Plans & Coupes** (Vue normale au plan, Plans XY, XZ, YZ, Plan d'étage, Plan personnalisé, Coupes 3D), **Aides Visuelles** (Grille, Niveaux, Règles, Nœuds, Étiquettes, Magnétisme grille/objets, Plein écran), **Fenêtres & Docks** (Arbre du Modèle, Propriétés, Résultats 3D, Visibilité, Console). |
| **8. Outils** | **Inspection** (Mesurer distance spatiale 3D), **Environnement** (Basculer thème Sombre / Clair), **Documentation** (Aide intégrée, Guide des raccourcis clavier, À propos de TSA). |

### Système de Panneaux Docks Centralisés (`WindowManager`)
L'espace de travail s'adapte à vos besoins grâce aux docks ancrables gérés de manière centralisée :
1. **Arbre du Modèle (`ModelTreeDock`)** : Hiérarchie complète des éléments (Nœuds, Poutres, Poteaux, Câbles, Dalles, Voiles, Niveaux, Grilles) avec synchronisation bidirectionnelle de sélection.
2. **Inspecteur des Propriétés (`PropertiesDock`)** : Édition contextuelle des sections, matériaux, cotes, excentrements, et volet d'inspection nodale des résultats OpenSees.
3. **Contrôle des Résultats 3D (`ResultsDockWidget`)** : Choix des sollicitations affichées, mode de déformée, slider temporel/incrémental, filtrage des nœuds et presets de cadrage.
4. **Calques & Visibilité (`VisibilityDock`)** : Masquage ou affichage fin des grilles, niveaux, cotes, repères et charges.
5. **Palette d'Éléments Structuraux (`StructuralElementsDock`)** : Accès direct pour le tracé interactif rapide.
6. **Plans de Travail & Caméra (`ProjectionViewDock`)** : Gestionnaire des plans de travail multiples et alignement caméra.
7. **Console de Diagnostics (`LogConsoleDock`)** : Suivi d'exécution en direct, sortie des solveurs et ligne de commande.

---

## 🏛️ Architecture Logicielle & Source de Vérité

TSA applique un principe architectural strict : **le modèle structural (`TSA::Model::Model`) est la source de vérité unique**.

```text
               +-------------------------------------------+
               |        Interface Utilisateur (Qt 6)       |
               |       Ribbon, Docks, Dialogs, Widgets     |
               +-------------------------------------------+
                                     |
                                     | Signaux / Commandes réversibles
                                     v
               +-------------------------------------------+
               |  Commands & Undo/Redo (ICommand, Manager) |
               |    Model Snapshots & Historique d'actions |
               +-------------------------------------------+
                                     |
                                     v
  =======================================================================
  ===   MODÈLE STRUCTURAL (TSA::Model::Model) — SOURCE DE VÉRITÉ      ===
  ===   Node, Beam, Column, Cable, Slab, Wall, Foundation, Section   ===
  =======================================================================
                   /                                   \
                  /                                     \
                 v                                       v
    +-------------------------+             +-------------------------+
    |  Constructeurs Géométrie |             |    Modèle d'Analyse     |
    |    (*Geometry OCCT)     |             |      (OpenSees)         |
    +-------------------------+             +-------------------------+
                 |                                       |
                 v                                       v
    +-------------------------+             +-------------------------+
    | Visualisation 3D (OCCT) |             |    Résultats de Calcul  |
    | AIS_Shape, OccView, V3d |             |   NVM, U, Déformée 3D   |
    +-------------------------+             +-------------------------+
```

* Aucun widget UI ne manipule directement la géométrie OpenCASCADE.
* Aucune forme OCCT (`TopoDS_Shape`) n'est utilisée pour stocker des données mécaniques.
* Les modifications d'état transitent par le système de commandes transactionnelles avec support complet Undo/Redo.

---

## 🧪 Tests Automatisés

Le projet intègre une suite rigoureuse de **84 bancs d'essais unitaires automatisés** (100% de réussite) validant la chaîne géométrique, mécanique et logicielle :

```powershell
# 1. Compilation de la suite de tests (Presets Ninja)
cmake --build --preset ninja-debug --target TSA_Tests

# 2. Exécution directe
.\build-ninja-debug\TSA_TestSuite.exe

# Ou exécution via CTest
ctest --test-dir build-ninja-debug --output-on-failure
```

### Couverture des 14 Suites de Tests :
* **Suites 1–8 :** Modélisation 3D solide B-Rep OCCT, système de câbles/haubans, format binaire `.tsa`, Undo/Redo transactionnel.
* **Suite 9 :** Système d'extensions **TSALib** (registre, chargement différé, cache multi-niveaux, packaging `.tsalib`).
* **Suite 10 :** Plans de travail 3D interactifs (WorkPlanes), transformations de repères, magnétisme spatial.
* **Suite 11 :** Gestionnaire de fenêtres centralisé (`WindowManager`) et persistance des dispositions d'écran.
* **Suite 12 :** Gestionnaire de nœuds centralisé, sélection et accrochage non-destructif.
* **Suite 13 :** Système de charges structurales, poids propre automatique, cas et combinaisons Eurocodes, scripts OpenSees.
* **Suite 14 :** Solveur OpenSees, immutabilité des snapshots de calcul, conditions d'appuis 3D (6 DDL), calculs multi-pas, diagrammes 3D $N, V, M$, détection des nœuds libres et synchronisation modèle/résultats.

---

## 📦 Système d'Extensions TSALib

TSA est doté du système d'extensions dynamiques d'ingénierie **TSALib** (`TSA::ExtensionSystem`) :
* **100% Découplé du Binaire :** Matériaux Eurocodes, profilés métalliques européens (IPE, HEA, HEB, UPN), sections personnalisées, câbles et textures PBR sont stockés au format ouvert JSON/PNG et modifiables sans recompiler le logiciel.
* **Rechargement à Chaud (Hot Reload) :** Actualisation instantanée du modèle 3D et des bibliothèques en un clic.
* **Packaging Autonome (`.tsalib`) :** Importation et exportation d'archives compressées autonomes signées par somme de contrôle SHA-256 avec protection anti-Path-Traversal.
* **Reproductibilité des Calculs :** Snapshots mécaniques scellés dans le fichier de projet `.tsa` (`CHUNK_SNAP`).

> 📖 **Pour en savoir plus, consultez le guide dédié : [docs/TSALIB_SYSTEM.md](docs/TSALIB_SYSTEM.md).**

---

## 🎮 Navigation & Raccourcis 3D

Les interactions et raccourcis clavier respectent scrupuleusement la norme internationale de documentation technique **IEEE Std 1063-2001** :

| Action / Commande | Raccourci Clavier / Souris |
| :--- | :--- |
| **Rotation / Orbite 3D** | Clic droit maintenu + Déplacement souris |
| **Panoramique (Pan)** | Clic molette maintenu + Déplacement souris |
| **Zoom avant / arrière** | Molette de la souris |
| **Centrer la vue (Fit All)** | <kbd>F</kbd> |
| **Cadrer la sélection** | <kbd>Maj</kbd> + <kbd>F</kbd> |
| **Vue isométrique initiale** | <kbd>R</kbd> |
| **Vue d'accueil (Home)** | <kbd>Origine (Home)</kbd> |
| **Vue de dessus (Plan XY)** | Pavé numérique <kbd>7</kbd> |
| **Vue de face (Plan XZ)** | Pavé numérique <kbd>1</kbd> |
| **Vue de droite (Plan YZ)** | Pavé numérique <kbd>3</kbd> |
| **Inspecteur des Propriétés** | <kbd>P</kbd> |
| **Lancer le Calcul EF** | <kbd>F5</kbd> |
| **Générateur Note de Calcul** | <kbd>F8</kbd> |
| **Plein Écran** | <kbd>F11</kbd> |
| **Aide contextuelle** | <kbd>F1</kbd> |

---

## 📁 Organisation du Dépôt

```text
TSA/
├── cmake/                      # Scripts CMake et déploiement automatique OCCT/DLLs
├── docs/                       # Guides d'architecture et spécifications techniques
│   ├── ARCHITECTURE.md         # Flux de données et principes de conception
│   ├── TSALIB_SYSTEM.md        # Spécification complète du système d'extensions TSALib
│   ├── TSA_FILE_FORMAT.md      # Spécification du format binaire .tsa (chunks)
│   ├── TSA_DIAGNOSTICS.md      # Journalisation, télémétrie et rapports de crash
│   ├── ANALYSIS_OPENSEES.md    # Intégration du solveur OpenSees et formulations EF
│   ├── OPENSEES_RESULTS.md     # Structure des résultats et diagrammes 3D
│   ├── UI.md                   # Architecture de l'interface graphique Qt 6
│   └── MODEL.md                # Spécification du modèle structural de données
├── Extensions/                 # Extensions installées (bibliothèque TSALib standard)
├── resources/                  # Ressources graphiques Qt (.qrc), icônes et branding
│   ├── branding/               # Logos officiels vectoriels, bannières et renders
│   └── icons/                  # Jeu d'icônes SVG pour le Ruban et les outils CAO
├── scripts/                    # Scripts PowerShell d'orchestration et détection d'outils
├── src/                        # Code source C++20 de l'application
│   ├── Analysis/               # Solveurs EF (OpenSees, modale, pushover, résultats)
│   ├── App/                    # Classe d'application principale et initialisation
│   ├── Commands/               # Commandes CAO réversibles (ICommand)
│   ├── Coordinate/             # Points 3D, niveaux d'étages et plans de travail
│   ├── Diagnostics/            # Moteur de logs, télémétrie et CrashHandler
│   ├── ExtensionSystem/        # Moteur TSALib (Registry, Loader, Validator, Cache)
│   ├── Geometry/               # Constructeurs géométriques solides B-Rep OCCT
│   ├── Grid/                   # Définition, rendu et magnétisme des grilles 3D
│   ├── IO/                     # Format de fichier .tsa binaire et snapshots
│   ├── Model/                  # Modèle structural source de vérité (Barres, Nœuds, Dalles...)
│   ├── NDC/                    # Moteur de génération des Notes de Calcul réglementaires
│   ├── UI/                     # Interface Qt 6 (Ruban 8 onglets, Docks, Dialogues)
│   ├── UndoRedo/               # Gestionnaire transactionnel Undo/Redo
│   └── Viewer/                 # Vue 3D OpenCASCADE (OccView, textures PBR, sélection)
├── tests/                      # Suite de tests unitaires automatisés (84 bancs d'essais)
├── tools/                      # Outils de vérification automatisée (raccourcis, intégrité)
├── CMakeLists.txt              # Configuration principale du build CMake
├── CMakePresets.json           # Presets de compilation Ninja et Visual Studio
└── run.bat                     # Lanceur intelligent tout-en-un
```

---

## ⚖️ Conformité Normative & Documentation

* **Eurocodes Structuraux :** Conception et combinaisons conformes aux normes **EN 1990** (Bases de calcul), **EN 1991** (Actions), **EN 1992** (Béton armé), **EN 1993** (Structures métalliques) et **EN 1998** (Calcul parasismique).
* **Documentation Technique :** Rédaction et structure conformes aux normes **IEEE Std 1063-2001 (R2007)** et **ISO/IEC/IEEE 26514:2022**.
* **Cycle de Vie Logiciel :** Adhésion aux exigences de qualité logicielle **ISO/IEC 25010** et d'ingénierie **ISO/IEC/IEEE 12207**.

---

## 👨‍💻 Auteur & Crédits

* **Concepteur & Développeur Principal :** Christinot TSARALOHA
* **Technologies Clés :** C++20 • Qt 6 • OpenCASCADE Technology • OpenSees • CMake • Ninja