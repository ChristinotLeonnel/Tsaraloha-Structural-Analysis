# Manuel d'Utilisation et Documentation Technique
# TSA - Tsaraloha Structural Analysis

---

## 📌 Table des Matières

1. [Présentation Générale](#1-présentation-générale)
2. [Architecture Technique & Modules](#2-architecture-technique--modules)
3. [Interface Utilisateur & Ergonomie](#3-interface-utilisateur--ergonomie)
4. [Interfaces Unifiées de Modélisation](#4-interfaces-unifiées-de-modélisation)
   - [4.1 Éléments Filaires (Barres, Poutres, Poteaux, Treillis)](#41-éléments-filaires-barres-poutres-poteaux-treillis)
   - [4.2 Éléments Surfaciques (Dalles, Planchers, Voiles, Murs)](#42-éléments-surfaciques-dalles-planchers-voiles-murs)
   - [4.3 Nœuds et Modules Primitifs 3D](#43-nœuds-et-modules-primitifs-3d)
5. [Système de Coordonnées, Grilles 3D et Niveaux d'Étage](#5-système-de-coordonnées-grilles-3d-et-niveaux-détage)
6. [Outils d'Édition, Manipulation 3D et Presse-Papier](#6-outils-dédition-manipulation-3d-et-presse-papier)
7. [Ligne de Commande Intégrée (CLI)](#7-ligne-de-commande-intégrée-cli)
8. [Guide des Raccourcis Clavier](#8-guide-des-raccourcis-clavier)
9. [Analyse EF, Calculs et Exploitation des Résultats](#9-analyse-ef-calculs-et-exploitation-des-résultats)
10. [Guide de Compilation et Déploiement](#10-guide-de-compilation-et-déploiement)
   - [10.1 Détection et Gestion Automatique des Compilateurs](#101-détection-et-gestion-automatique-des-compilateurs)
   - [10.2 Installation Sécurisée de MinGW-w64](#102-installation-sécurisée-de-mingw-w64)
   - [10.3 Compatibilité des Toolchains (Qt 6, OCCT, VTK)](#103-compatibilité-des-toolchains-qt-6-occt-vtk)
   - [10.4 Téléchargement Automatisé des Bibliothèques Tierces](#104-téléchargement-automatisé-des-bibliothèques-tierces)
   - [10.5 Répertoires Non-Destructifs et Compilation](#105-répertoires-non-destructifs-et-compilation)
   - [10.6 Déploiement et Exécution](#106-déploiement-et-exécution)

---

## 1. Présentation Générale

**TSA (Tsaraloha Structural Analysis)** est une suite logicielle avancée de conception assistée par ordinateur (CAO) et d'analyse par éléments finis (EF) dédiée au génie civil et aux structures de bâtiment.

### Capacités Principales :
- **Noyau Géométrique Puissant** : Basé sur **OpenCASCADE Technology (OCCT 8.0.1)** pour la représentation B-Rep exacte de solides 3D réels (extrusions de profilés, congés, ouvertures, plaques épaisses).
- **Interface Graphique Moderne** : Conçue avec **Qt 6** combinant un ruban fluide façon AutoCAD / Autodesk Robot, des règles métriques de viewport synchronisées, et des panneaux d'inspection dockables.
- **Modélisation Structurale Centralisée** : Gestion rigoureuse des éléments 1D (poutres, poteaux, barres de treillis, tirants), éléments 2D (dalles planes, radiers, voiles banchés, murs porteurs) et appuis (encastrements, rotules, appuis simples).
- **Interfaces Unifiées Modeless** : Boîtes d'outils interactives flottantes permettant de paramétrer et dessiner simultanément en direct dans le viewport 3D.
- **Calculs Éléments Finis** : Discrétisation automatique, résolution linéaire statique $[K]\{u\} = \{F\}$, analyse modale dynamique et affichage cartographique des contraintes et déformées.

---

## 2. Architecture Technique & Modules

Le code source est segmenté en sous-systèmes modulaires et découplés :

```text
TSA/
├── src/
│   ├── App/          # Point d'entrée applicatif, gestion du cycle de vie
│   ├── Coordinate/   # Systèmes de coordonnées, niveaux d'étages et transformations
│   ├── Geometry/     # Génération des géométries solides OCCT (sections, voiles, dalles)
│   ├── Grid/         # Définition, rendu et magnétisme (Snap) des grilles 3D
│   ├── Model/        # Modèle de données structural (Nœuds, Barres, Dalles, Voiles, Matériaux)
│   ├── UI/           # Fenêtre principale, Ruban, Docks, Dialogues interactifs
│   └── Viewer/       # Fenêtre de rendu OpenGL OpenCASCADE (OccView), caméra et sélection
```

### Description des Modules Clés :

| Module | Description |
|---|---|
| `TSA::Model` | Entités physiques du bâtiment (Node, Beam, Column, Slab, Wall, Material, Section). Intègre un gestionnaire d'historique Undo/Redo à transactions complètes. |
| `TSA::Viewer::OccView` | Widget OpenGL encapsulant la vue `V3d_View` et le contexte interactif `AIS_InteractiveContext`. Gère l'accrochage curseur, le rubberband de dessin et le lancer de rayon 3D. |
| `TSA::Grid` | Grilles cartésiennes et polaires/cylindriques avec étiquettes d'axes 3D vectorielles et moteur de magnétisme intelligent (`GridSnapManager`). |
| `TSA::Coordinate` | Gestionnaire d'étages (`LevelManager`). La modification d'une cote d'élévation propage immédiatement la mise à jour aux poteaux et voiles attachés. |
| `TSA::UI::Ribbon` | Composants de ruban ergonomique (`RibbonBar`, `RibbonTab`, `RibbonPanel`, `RibbonButton`). |

---

## 3. Interface Utilisateur & Ergonomie

L'interface de TSA est organisée en 5 zones principales :

```
+-------------------------------------------------------------------------+
| Barre de Titre & Menus Déroulants (Fichier, Édition, Affichage, Dessin) |
+-------------------------------------------------------------------------+
| Ruban Principal (Accueil, Structure & Sections, Analyse, Affichage)    |
+-------------------------------------------------------------------------+
| Règles Supérieures & Barre de Sélection d'Étage Actif                  |
+-------------------+---------------------------------+-------------------+
|                   |                                 |                   |
|  Arborescence     |     Viewport 3D OpenCASCADE     |   Inspecteur      |
|  du Modèle        |     Rendu B-Rep Solide / Filaire|   des Propriétés  |
|  (ModelTreeDock)  |     Manipulation temps réel     |   (PropertyDock)  |
|                   |                                 |                   |
+-------------------+---------------------------------+-------------------+
| Console de Logs & Ligne de Commande Rapide (CLI)                       |
+-------------------------------------------------------------------------+
| Barre d'État (Coordonnées 3D du curseur, Magnétisme, Mode actif)       |
+-------------------------------------------------------------------------+
```

1. **Ruban Principal** : Accès thématique à tous les outils avec séparateurs et icônes haute résolution.
2. **Viewport 3D** : Affichage temps réel accéléré matériellement par OpenGL, avec ombres douces et antialiasing.
3. **Arborescence du Modèle (Gauche)** : Hiérarchie complète des éléments (Nœuds, Poutres, Poteaux, Dalles, Voiles, Niveaux, Grilles).
4. **Inspecteur des Propriétés (Droite)** : Modification paramétrique instantanée de tout objet sélectionné.
5. **Console & CLI (Bas)** : Historique des opérations et boîte de saisie pour commandes rapides type AutoCAD.

---

## 4. Interfaces Unifiées de Modélisation

Conformément aux standards professionnels de modélisation structurale, TSA regroupe la création des entités dans deux fenêtres unifiées flottantes et non-bloquantes (Modeless) :

---

### 4.1 Éléments Filaires (Barres, Poutres, Poteaux, Treillis)

Accessible via le raccourci <kbd>Alt+B</kbd> ou le ruban **Structure > Éléments Filaires**.

Cette interface unique centralise tous les éléments 1D linéaires :

- **Rôles disponibles** :
  - `Poutre` : Élément horizontal ou incliné travaillant en flexion / cisaillement / torsion.
  - `Poteau` : Élément vertical supportant principalement les charges axiales et le flambement.
  - `Barre (Générique)` : Barre spatiale à 6 degrés de liberté par nœud.
  - `Treillis` : Barre articulée travaillant uniquement en traction/compression axiale.
  - `Diagonale / Contreventement` : Barre de stabilité latérale.
  - `Tirant` : Élément précontraint ou suspendu.
- **Profilés et Sections Transversales** :
  - Profilés laminés normalisés : IPE, HEA, HEB (de 80 à 600).
  - Profilés paramétriques en béton armé ou bois : Rectangulaire ($b \times h$), Circulaire ($\varnothing$).
  - Bouton `...` pour ouvrir le gestionnaire de sections personnalisées.
- **Matériaux** : Association instantanée (Acier S235, S355, Béton C25/30, etc.).
- **Modes de Tracé** :
  1. **Tracé interactif à la souris** : Cliquez sur un premier nœud/intersection dans la vue 3D, déplacez le curseur pour voir le profilé solide extrudé en temps réel, puis cliquez sur l'extrémité.
  2. **Option "Étirer"** : Enchaîne automatiquement le point de départ de la barre suivante sur la fin de la précédente.
  3. **Saisie numérique directe** : Renseignez les coordonnées $X; Y; Z$ ou les numéros de nœuds dans les champs Origine et Extrémité puis cliquez sur **Ajouter**.
- **Orientation & Excentrement** :
  - Rotation de section $\gamma$ : de 0° à 360°.
  - Excentrement de l'axe : Fibre supérieure, inférieure, gauche, droite ou centré.

---

### 4.2 Éléments Surfaciques (Dalles, Planchers, Voiles, Murs)

Accessible via le raccourci <kbd>Alt+S</kbd> ou le ruban **Structure > Éléments Surfaciques**.

Cette interface unifie l'ensemble des éléments 2D de surface sous une même ergonomie :

#### A. Dalle / Plancher
- **Épaisseur $e$** : Réglable de 0.01 m à 5.00 m (par défaut 0.20 m).
- **Comportement Structural** :
  - `Bidirectionnelle (2 sens)` : Portance dans les deux directions principales (ex: dalle pleine sur 4 appuis).
  - `Unidirectionnelle (1 sens)` : Report de charge préférentiel sur poutres maîtresses (ex: plancher poutrelle-hourdis).
- **Matériau & Couleur** : Nuancier interactif et sélection de la classe de béton (C25/30, C30/37, etc.).
- **Définition du Contour Polygonal** :
  - Cliquez sur au moins 3 nœuds successifs dans le viewport 3D.
  - La liste des nœuds s'actualise en temps réel dans la fenêtre.
  - Pour fermer et créer la dalle :
    - Recliquez sur le nœud initial, **OU**
    - Faites un **clic droit** dans le viewport, **OU**
    - Appuyez sur **Entrée** ou le bouton **Fermer le contour**.
  - Saisie directe : Vous pouvez également taper la liste des nœuds (ex: `1, 2, 4, 3`) et cliquer sur **Ajouter**.

#### B. Voile / Mur Porteur
- **Épaisseur $e$** : Par exemple 0.20 m.
- **Hauteur $H$** : Hauteur totale du voile (par défaut 3.00 m ou calée sur l'étage).
- **Excentrement** : Décalage latéral de l'axe neutre par rapport au tracé de base.
- **Tracé 3D** :
  - Cliquez sur le nœud de base d'origine puis sur le nœud d'extrémité.
  - Le solide 3D du voile en béton est automatiquement généré et orienté verticalement.
  - Option **Étirer** cochée : permet de tracer une cage d'escalier ou un noyau de contreventement continu clic après clic.

---

### 4.3 Nœuds et Modules Primitifs 3D

- **Nœud Spatio-Temporel (<kbd>N</kbd>)** : Création interactive par clic ou par coordonnées précises.
- **Cube Structural 3D** : Génère en un seul clic une travée complète (8 nœuds, 4 poteaux, 8 poutres et 1 plancher dalle) pour démarrer rapidement une étude.

---

## 5. Système de Coordonnées, Grilles 3D et Niveaux d'Étage

### Grilles 3D Paramétriques :
- **Grilles Cartésiennes** : Espacements modulables selon $X$, $Y$, $Z$ avec étiquetage alphabétique ($A, B, C...$) et numérique ($1, 2, 3...$).
- **Grilles Cylindriques / Polaires** : Rayons, angles et cercles concentriques pour structures circulaires ou réservoirs.
- **Magnétisme Dynamique (Snap)** : Détection automatique des intersections de grille, milieux, extrémités et nœuds existants avec indicateur visuel flottant.

### Gestionnaire de Niveaux d'Étage (Story Levels) :
- Permet de découper la structure en niveaux (RDC, R+1, R+2, Toiture).
- La barre supérieure de niveau permet de basculer instantanément le plan de travail actif.
- Toute élévation de niveau modifiée dans le gestionnaire met à jour automatiquement la hauteur des poteaux et voiles correspondants.

### Plans de Coupe 3D Dynamiques (Section Cut) :
- Coupe interactive selon les plans $XY$, $XZ$ ou $YZ$.
- Curseur de découpe permettant d'explorer l'intérieur d'un bâtiment multi-étages sans masquer manuellement les toitures ou planchers.

---

## 6. Outils d'Édition, Manipulation 3D et Presse-Papier

TSA offre des outils géométriques avancés inspirés des logiciels de CAO majeurs :

- **Sélection par Fenêtre & Clic (<kbd>Échap</kbd>)** :
  - Fenêtre de gauche à droite : sélectionne les éléments entièrement inclus.
  - Fenêtre de droite à gauche : sélectionne tous les éléments intersectés.
  - Touche <kbd>Ctrl</kbd> maintenue : ajout / retrait de la sélection.
- **Déplacement 3D Direct Point-à-Point** : Cliquez un point de référence puis le point cible.
- **Copie 3D Translation** : Duplique les éléments sélectionnés d'un vecteur spatial défini.
- **Rotation 3D Spatiale** : Définition du centre de rotation et de l'angle.
- **Presse-Papier Structural Intelligent (<kbd>Ctrl+C</kbd> / <kbd>Ctrl+V</kbd>)** :
  - Copie l'ensemble sélectionné (nœuds, profilés, sections associées, dalles).
  - Lors du collage, cliquez à l'endroit désiré dans le viewport 3D pour déposer le sous-ensemble avec ses connectivités intactes.
- **Historique Undo / Redo (<kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd>)** : Annulation et rétablissement à niveaux illimités pour toutes les opérations de modélisation.

---

## 7. Ligne de Commande Intégrée (CLI)

La console inférieure dispose d'un interpréteur de commandes en langage naturel et raccourcis :

| Commande | Raccourcis Alternatifs | Action |
|---|---|---|
| `WIRE` | `FILAIRE`, `BAR`, `BARRE` | Ouvre l'interface unifiée des Éléments Filaires |
| `BEAM` | `B`, `POUTRE` | Ouvre l'interface filaire préconfigurée en mode Poutre |
| `COLUMN` | `C`, `POTEAU` | Ouvre l'interface filaire préconfigurée en mode Poteau |
| `SURF` | `SURFACE` | Ouvre l'interface unifiée des Éléments Surfaciques |
| `SLAB` | `L`, `DALLE` | Ouvre l'interface surfacique en mode Dalle |
| `WALL` | `W`, `VOILE` | Ouvre l'interface surfacique en mode Voile |
| `NODE` | `N`, `NOEUD` | Active le mode de création de nœuds 3D |
| `SELECT` | `ESC`, `SEL` | Réinitialise le mode actif vers le mode Sélection |
| `FIT` | `F`, `ZOOM` | Cadre l'intégralité du modèle dans la fenêtre (Fit All) |
| `RESET` | `HOME` | Réinitialise la caméra en perspective isométrique |
| `MESH` | `MAILLAGE` | Lance la discrétisation par éléments finis |
| `SOLVE` | `CALC`, `RUN` | Exécute la résolution du calcul statique linéaire |
| `MODAL` | `FREQ` | Lance l'analyse modale dynamique |
| `DISP` | `DEPLACEMENT` | Affiche la cartographie des déplacements nodaux |
| `FORCES` | `DIAGRAM` | Affiche les diagrammes d'efforts internes ($M, N, V$) |
| `STRESS` | `VONMISES` | Affiche les contraintes équivalentes de Von Mises |

---

## 8. Guide des Raccourcis Clavier

### Navigation 3D dans le Viewport :
- **Rotation / Orbite 3D** : Clic droit maintenu + déplacement de la souris.
- **Panoramique (Pan)** : Clic molette maintenu + déplacement.
- **Zoom Avant / Arrière** : Molette de la souris.
- **Centrer la Vue (Fit All)** : Touche <kbd>F</kbd>.
- **Réinitialiser l'Orientation** : Touche <kbd>R</kbd>.

### Modélisation & Dessin :
- **Sélection / Annuler l'action courante** : <kbd>Échap</kbd>
- **Créer un Nœud** : <kbd>N</kbd>
- **Éléments Filaires (Barres, Poutres, Poteaux)** : <kbd>Alt+B</kbd>
- **Éléments Surfaciques (Dalles, Voiles)** : <kbd>Alt+S</kbd>
- **Poutre Rapide** : <kbd>B</kbd>
- **Poteau Rapide** : <kbd>C</kbd>
- **Dalle Rapide** : <kbd>L</kbd>
- **Voile Rapide** : <kbd>W</kbd>

### Édition & Système :
- **Annuler** : <kbd>Ctrl+Z</kbd>
- **Rétablir** : <kbd>Ctrl+Y</kbd>
- **Copier au Presse-papier** : <kbd>Ctrl+C</kbd>
- **Coller dans la Vue 3D** : <kbd>Ctrl+V</kbd>
- **Supprimer la Sélection** : <kbd>Suppr</kbd>
- **Lancer le Calcul EF** : <kbd>F5</kbd>
- **Plein Écran** : <kbd>F11</kbd>
- **Aide Intégrée** : <kbd>F1</kbd>

---

## 9. Analyse EF, Calculs et Exploitation des Résultats

### 1. Maillage des Éléments Finis :
- **Barres 1D** : Éléments poutres d'Euler-Bernoulli ou de Timoshenko à 2 nœuds et 12 degrés de liberté (DDL).
- **Dalles & Voiles 2D** : Maillage automatique par plaques minces de Kirchhoff ou coques de Mindlin-Reissner (éléments quadrangulaires Q4 / triangulaires T3).

### 2. Cas de Charges et Combinaisons :
- Charges nodales ponctuelles ($F_x, F_y, F_z, M_x, M_y, M_z$).
- Charges réparties trapézoïdales ou uniformes sur les barres ($q_z, q_y$).
- Charges surfaciques uniformes ($kN/m^2$) sur les dalles et planchers.
- Combinaisons de charges ELU (État Limite Ultime : $1.35 G + 1.5 Q$) et ELS (État Limite de Service : $G + Q$).

### 3. Résolution & Résultats :
- **Déplacements** : Représentation de la déformée réelle ou amplifiée avec échelle réglable.
- **Efforts Internes** : Diagrammes le long des barres des moments fléchissants $M_y$, $M_z$, de l'effort normal $N$ et de l'effort tranchant $V_z$.
- **Contraintes** : Cartographie en dégradé de couleurs des contraintes normales $\sigma$, de cisaillement $\tau$ et des contraintes équivalentes de Von Mises sur les dalles et voiles.

---

## 10. Guide de Compilation et Déploiement

### Prérequis Système :
- **Système** : Windows 10 ou Windows 11 (architecture **x64** obligatoire).
- **Compilateur** :
  - **MSVC** (Visual Studio 2022 / 2026 x64) avec support complet standard **C++20** *(recommandé, sélectionné par défaut si installé)*.
  - **MinGW-w64** (GCC 13+ / 16+ x64) avec POSIX threads et SEH exceptions *(sélectionné automatiquement si MSVC est absent, ou téléchargé à la volée)*.
- **Outils** : CMake 3.20+ et Ninja ou MSBuild.
- **Bibliothèques** :
  - Qt 6.2+ (`Core`, `Gui`, `Widgets`, `Svg`) — *ex. `C:\Qt\6.11.2\msvc2022_64` ou `C:\Qt\6.11.2\mingw_64`*.
  - OpenCASCADE Technology 8.0.1 (**téléchargé et installé automatiquement par CMake** si non présent).
  - Dépendances tierces 3rdparty (**téléchargées automatiquement par CMake** si non présentes).

---

### 10.1 Détection et Gestion Automatique des Compilateurs

Le projet intègre un ensemble de scripts PowerShell situés dans le dossier `scripts/` assurant une configuration 100% automatisée sans intervention manuelle :

- **Script principal d'orchestration : [`scripts/setup_build.ps1`](file:///e:/Book/Dev/TSA/scripts/setup_build.ps1)**
  - Coordonne la détection, la validation par compilation test C++20, la vérification de compatibilité Qt / OCCT, et la génération CMake.
- **Détection des compilateurs : [`scripts/detect_compiler.ps1`](file:///e:/Book/Dev/TSA/scripts/detect_compiler.ps1)**
  - Interroge les outils Microsoft (`vswhere.exe`) pour localiser Visual Studio ou Visual Studio Build Tools, l'environnement `vcvarsall.bat` x64 et le Windows SDK.
  - Vérifie la présence de `cl.exe` même s'il n'est pas déclaré dans le `PATH` global de Windows.
  - Si MSVC est présent, il est **toujours prioritaire** et sélectionné (`Compiler sélectionné : MSVC`).
  - Si MSVC est absent, il recherche un environnement **MinGW-w64** existant (`gcc.exe`, `g++.exe` dans le `PATH`, `C:\TSA\tools\mingw64\bin`, `C:\Qt\Tools\mingw*`, etc.).

---

### 10.2 Installation Sécurisée de MinGW-w64

Si **aucun compilateur fonctionnel n'est détecté** sur la machine hôte :

- Le script [`scripts/install_mingw.ps1`](file:///e:/Book/Dev/TSA/scripts/install_mingw.ps1) prend le relais automatiquement.
- **Règle d'or** : Il ne télécharge **jamais** MinGW si un environnement MSVC fonctionnel est déjà disponible.
- **Source fiable et maintenue** : Utilise la distribution officielle **WinLibs MinGW-w64 GCC 16.2.0 + UCRT + SEH (x86_64)** depuis GitHub Releases.
- **Contrôle d'intégrité strict** : Calcule l'empreinte **SHA256** du fichier téléchargé et la compare au condensat officiel (`c1f52294597c0b73786b2a78eb5d176d89226d2f21875eab75e783a8b1cefcc4`). En cas de non-concordance, le fichier est détruit et le script s'arrête avec un code d'erreur non nul.
- **Emplacement contrôlé** : Installé par défaut dans `C:\TSA\tools\mingw64` (ou repli automatique dans `%LOCALAPPDATA%\TSA\tools\mingw64` si les droits d'administration sur `C:\` sont insuffisants).
- **Configuration PATH** : Injecte le dossier `bin` dans la variable d'environnement `PATH` de la session courante sans créer de doublon.

---

### 10.3 Compatibilité des Toolchains (Qt 6, OCCT, VTK)

En C++, le mélange arbitraire d'ABI (Application Binary Interface) provoque des erreurs de liaison irrémédiables. Le script [`scripts/detect_qt.ps1`](file:///e:/Book/Dev/TSA/scripts/detect_qt.ps1) et [`scripts/detect_dependencies.ps1`](file:///e:/Book/Dev/TSA/scripts/detect_dependencies.ps1) appliquent une politique stricte :

| Compilateur Sélectionné | Toolchain Qt Requise | Toolchain OCCT Requise |
|---|---|---|
| **MSVC** | `Qt 6 MSVC` (ex. `msvc2022_64`) | `OCCT MSVC` (`opencascade-8.0.1-vc14-64`) |
| **MinGW** | `Qt 6 MinGW` (ex. `mingw_64`) | `OCCT MinGW` |

Si la version de Qt détectée ne correspond pas au compilateur sélectionné, le système stoppe immédiatement la configuration et affiche l'erreur explicite :
```text
Erreur : la version de Qt détectée n'est pas compatible avec le compilateur sélectionné.
```

---

### 10.4 Téléchargement Automatisé des Bibliothèques Tierces

Grâce au module CMake [`cmake/SetupDependencies.cmake`](file:///e:/Book/Dev/TSA/cmake/SetupDependencies.cmake) :
- Si `opencascade-8.0.1-vc14-64` ou `3rdparty-vc14-64` ne sont pas présents localement (ex. clone Git frais), CMake télécharge automatiquement les archives officielles de **OCCT 8.0.1** (~53 Mo) et **3rdparty** (~188 Mo) depuis GitHub Releases avec affichage de la progression en direct (`SHOW_PROGRESS`).
- Les archives imbriquées sont décompressées directement à la racine du projet et les fichiers d'archive temporaires sont purgés.
- Cette automatisation peut être désactivée si nécessaire via `-DTSA_AUTO_DOWNLOAD_DEPS=OFF`.

---

### 10.5 Répertoires Non-Destructifs et Compilation

Afin d'éviter tout conflit de cache CMake lorsque plusieurs compilateurs sont utilisés sur la même machine, le système n'écrase jamais le dossier `build/` existant :
- Les configurations MSVC ciblent le dossier dédié **`build-msvc/`**.
- Les configurations MinGW ciblent le dossier dédié **`build-mingw/`**.

#### Commandes d'utilisation courante :

```powershell
# 1. Configuration automatique (MSVC prioritaire, MinGW en repli)
powershell -ExecutionPolicy Bypass -File .\scripts\setup_build.ps1

# 2. Configuration ET compilation immédiate (Release)
powershell -ExecutionPolicy Bypass -File .\scripts\setup_build.ps1 -Build

# 3. Compilation en mode Debug
powershell -ExecutionPolicy Bypass -File .\scripts\setup_build.ps1 -Config Debug -Build

# 4. Forcer l'utilisation de MinGW-w64 (test de la branche MinGW)
powershell -ExecutionPolicy Bypass -File .\scripts\setup_build.ps1 -ForceMinGW
```

---

### 10.6 Déploiement et Exécution

À la fin de la compilation, la commande post-build CMake [`cmake/DeployDependencies.cmake`](file:///e:/Book/Dev/TSA/cmake/DeployDependencies.cmake) déploie automatiquement :
1. Les 74 DLLs OpenCASCADE 8.0.1.
2. L'ensemble des DLLs tierces 3rdparty (FreeType, TBB, FreeImage, Jemalloc).
3. Toutes les dépendances et plugins de rendu Qt 6 via `windeployqt`.

#### Lancement de l'application :

- **Via le script batch configuré** :
  ```cmd
  run.bat
  ```
- **Ou directement via l'exécutable autonome** :
  ```powershell
  # Pour un build MSVC :
  .\build-msvc\Release\TSA.exe

  # Pour un build MinGW :
  .\build-mingw\TSA.exe
  ```

---
*Documentation rédigée pour TSA - Tsaraloha Structural Analysis. Tous droits réservés.*
