# Système de Coordonnées, Niveaux et Plans de Travail — TSA
**Conforme aux normes IEEE Std 1063-2001 (R2007) & ISO/IEC/IEEE 26514:2022**

| Métadonnée | Valeur |
| :--- | :--- |
| **Document** | Spécification technique du système de coordonnées, niveaux et plans de travail |
| **Identifiant du document** | `TSA-SPEC-COORD-2026.10` |
| **Version du document** | 2.0.0 |
| **Version logicielle cible** | TSA v0.1.0 (`main`) |
| **Date d'émission** | 03 Octobre 2026 |
| **Auteur / Émetteur** | Équipe d'Ingénierie & Architecture TSA |
| **Statut** | Approuvé / Référence Officielle |
| **Classification** | Architecture & Spécification Technique |

> Document vivant : en cas de divergence, l'implémentation C++ dans `src/Coordinate/`, `src/Viewer/` et `src/Model/` fait foi.

---

## 1. Portée et Principes Directeurs

Le module `src/Coordinate/` fournit l'infrastructure spatiale unique et centralisée pour :
1. Le positionnement géométrique des primitives structurelles (`Point3D`, conversion vers OpenCASCADE `gp_Pnt`).
2. Les trames et grilles cartésiennes X/Y et cylindriques $R/\theta/Z$.
3. La gestion altimétrique des étages (`Level`, `LevelManager`).
4. Le système tridirectionnel de **Plans de Travail (WorkPlane)** selon les axes **Z**, **X** et **Y**, incluant la détection automatique structurelle et le mode 2D CAD avec isolation géométrique.

### Règle d'Unicité Spatiale
Toute coordonnée de nœud, de plan ou de grille dans TSA dérive obligatoirement de ce système central. Aucun widget d'interface ni composant d'affichage ne stocke de coordonnées géométriques indépendantes.

---

## 2. Primitives Géométriques (`src/Coordinate/Point3D`)

La classe `Point3D` encapsule les coordonnées réelles $(x, y, z)$ en double précision :
- **Interfaçage OpenCASCADE transparent** :
  - Constructeur de conversion `Point3D(const gp_Pnt& p)` ;
  - Opérateur de conversion implicite `operator gp_Pnt() const` et méthode explicite `toGpPnt()` ;
  - Utilise l'allocation compacte directe sans surcoût mémoire.
- **Opérations géométriques et métriques** :
  - `distance(const Point3D& other) const` : distance euclidienne 3D $\sqrt{\Delta x^2 + \Delta y^2 + \Delta z^2}$ ;
  - `distanceXY(const Point3D& other) const` : distance projetée dans le plan horizontal $\sqrt{\Delta x^2 + \Delta y^2}$ ;
  - `isAlmostEqual(const Point3D& other, double tol = 1e-4) const` : prédicat de proximité avec tolérance absolue ;
  - Surcharges complètes des opérateurs arithmétiques vectoriels (`+`, `-`, `*`, `/`).

---

## 3. Grilles Cartésiennes et Coordonnées Cylindriques

### 3.1 `CoordinateSystem` (`src/Coordinate/CoordinateSystem.h`)
Classe dérivant de `QObject`, source de vérité des trames et espacements :
- **Axe X** : `xPositions()`, `setXPositions(positions)`, `setXSpacings(startX, spacings)`, `getXSpacings()`. Les libellés d'axes numériques (`1, 2, 3...`) sont synchronisés automatiquement (`m_xLabels`).
- **Axe Y** : `yPositions()`, `setYPositions(positions)`, `setYSpacings(startY, spacings)`, `getYSpacings()`. Les libellés alphabétiques (`A, B, C...`) sont synchronisés via `indexToLetter` (`m_yLabels`).
- **Axe Z** : Entièrement délégué au `LevelManager` (`zLevels()`, `zCount()`, `setZLevels()`) afin de garantir l'absence de duplication d'états entre les grilles et les étages.
- **Signal Qt** : Émission de `coordinatesChanged()` à chaque modification de position ou d'espacement.

### 3.2 Coordonnées Cylindriques (`src/Coordinate/CylindricalCoordinates.h`)
Permet la modélisation circulaire, polygonale ou radiale (structures réservoirs, tours, silos) :
- Rayons $R$ : `radii()`, `setRadii(rList)`.
- Angles polaires $\theta$ : `anglesDeg()`, `setAnglesDeg(aList)` (en degrés décimaux).
- Niveaux $Z$ : `zLevels()`, `setZLevels(zList)`.
- Conversions et projections :
  - `toCartesian(r, thetaDeg, z)` : calcule le `Point3D` cartésien correspondant.
  - `gridPoint(ir, itheta, iz)` : retourne le nœud de grille polaire discrétisé.
  - `findClosestPolar(worldPnt, tol, outPnt)` : projection géométrique la plus proche.

---

## 4. Gestionnaire d'Étages et Niveaux (`src/Coordinate/Level` & `LevelManager`)

### 4.1 Structure `Level`
```cpp
struct Level {
    std::string id;       // UUID ou identifiant alphanumérique unique
    std::string name;     // Nom usuel (ex: "RDC", "R+1", "Toiture")
    double elevation;     // Cote altimétrique Z en mètres
    bool visible;         // Indicateur d'affichage de la trame d'étage
};
```

### 4.2 `LevelManager`
Classe `QObject` administrant la liste ordonnée des niveaux :
- Gestion CRUD : `addLevel(name, elevation)`, `addLevelWithId(id, name, elevation)`, `removeLevel(id)`, `setLevelElevation(id, newElevation)`, `setLevelName(id, newName)`.
- Recherche géométrique : `findLevelAtElevation(z, tolerance = 1e-3)`, `findClosestLevel(z)`.
- Signaux Qt émis :
  - `levelAdded(const std::string& levelId)`
  - `levelRemoved(const std::string& levelId)`
  - `levelModified(const std::string& levelId)`
  - `levelElevationChanged(const std::string& levelId, double oldElevation, double newElevation)`
  - `levelsChanged()`

### 4.3 Règle de Déplacement Altimétrique Sélectif des Nœuds
Lors de la modification de l'élévation d'un niveau via `Model::onLevelElevationChanged` :
- **Seuls les nœuds explicitement rattachés** (`node.levelId() == levelId`) sont translatés le long de l'axe Z : $Z_{nouveau} = Z_{ancien} + (Elevation_{nouvelle} - Elevation_{ancienne})$.
- Les nœuds appartenant à d'autres niveaux ou non rattachés conservent strictement leurs coordonnées.
- Tous les éléments linéaires et surfaciques connectés aux nœuds déplacés voient leur géométrie recalculée.
- Une notification unique groupée `notifyModelDiffApplied(diff)` est émise vers les observateurs.

---

## 5. Système Tridirectionnel de Plans de Travail (WorkPlane X-Y-Z)

Le système de plan de travail supporte le dessin, l'accrochage et l'isolation selon les trois axes directeurs de l'espace tridimensionnel :

```cpp
enum class WorkPlaneAxis {
    Z = 0, // Horizontal XY (planchers / niveaux d'étages)
    X = 1, // Vertical YZ (coupes transversales / pignons)
    Y = 2  // Vertical XZ (coupes longitudinales / façades)
};
```

### 5.1 Détection Automatique des Plans Structurels (`detectStructuralPlanes`)
La méthode `Model::detectStructuralPlanes(WorkPlaneAxis axis)` (déléguée à `CoordinateSystem::detectStructuralPlanes`) regroupe dynamiquement les plans pertinents selon l'axe sélectionné :

1. **Axe Z (Plans horizontaux)** :
   - Extrait en priorité tous les niveaux définis dans `LevelManager` (`lvl.elevation`, `lvl.name`).
   - Si aucun niveau n'est défini, extrait les altitudes $Z$ uniques des nœuds du modèle en les regroupant selon la tolérance normalisée `planeMembership = 0.05 m` (5 cm).
2. **Axe X (Coupes verticales transversales YZ)** :
   - Extrait les positions de grille cartésienne $X$ avec leurs libellés (ex: `Axe 1 (X = 0.00 m)`).
   - Complète avec les coordonnées $X$ uniques des nœuds du modèle pour inclure les plans structurels non alignés sur la grille.
3. **Axe Y (Coupes verticales longitudinales XZ)** :
   - Extrait les positions de grille cartésienne $Y$ avec leurs libellés (ex: `Axe A (Y = 0.00 m)`).
   - Complète avec les coordonnées $Y$ uniques des nœuds du modèle.

Chaque plan détecté est encapsulé dans `DetectedPlaneInfo` :
```cpp
struct DetectedPlaneInfo {
    WorkPlaneAxis axis = WorkPlaneAxis::Z;
    double offset = 0.0; // Cote altimétrique ou abscisse du plan (en m)
    std::string name;    // Libellé formaté (ex: "R+1 (Z = 3.00 m)", "Axe 2 (X = 5.00 m)")
    std::string id;      // Identifiant unique
};
```

### 5.2 Alignement Caméra et Vue Normale (`viewNormalToWorkPlane`)
Lors de l'activation d'un plan ou en mode 2D, la caméra 3D s'oriente rigoureusement perpendiculairement au plan de travail avec un vecteur d'orientation verticale (Up) cohérent :

| Axe du Plan | Type de Vue | Normale de Projection (`SetProj`) | Vecteur Haut (`SetUp`) | Effet Visuel |
| :--- | :--- | :--- | :--- | :--- |
| **Z** | Plan horizontal XY | $\vec{n} = (0, 0, 1)$ | $\vec{up} = (0, 1, 0)$ | Vue de dessus (Top view / plan d'étage) |
| **X** | Coupe verticale YZ | $\vec{n} = (1, 0, 0)$ | $\vec{up} = (0, 0, 1)$ | Vue latérale / pignon (vue vers $+X$, $Z$ en haut) |
| **Y** | Coupe verticale XZ | $\vec{n} = (0, 1, 0)$ | $\vec{up} = (0, 0, 1)$ | Vue frontale / élévation (vue vers $+Y$, $Z$ en haut) |

---

## 6. Mode 2D CAD et Isolation Géométrique

### 6.1 Activation du Mode 2D (`setMode2D(true)`)
L'activation de la case **[x] 2D** dans le bandeau supérieur de `ViewportContainer` enclenche une séquence déterministe conforme aux logiciels de CAO professionnels :
1. **Sauvegarde non-destructive de l'état 3D** :
   - Copie intégrale de l'état de la caméra 3D perspective (`Graphic3d_Camera`).
   - Mémorisation de l'état d'affichage initial de chaque objet interactif AIS.
2. **Basculement en projection orthographique** :
   - Élimine la distorsion de perspective via `m_viewManager.setOrthographic(true)`.
3. **Orientation automatique de la caméra** :
   - Alignement perpendiculaire strict selon la normale et le vecteur Up du plan actif via `viewNormalToWorkPlane()`.
4. **Affichage du plan de travail** :
   - Rendu de la grille locale du WorkPlane avec coloration normalisée des axes.
5. **Isolation géométrique automatique** :
   - Filtrage spatial via `updateElementIsolation()` avec tolérance centrale `GeometryTolerance::planeMembership = 0.05 m`.
   - Seuls les nœuds, barres, dalles et voiles situés sur le plan actif ($\pm 5\text{ cm}$) demeurent visibles. Tous les éléments hors plan sont masqués.
6. **Cadrage automatique (`FitAll`)** :
   - Recadrage dynamique restreint aux éléments visibles du plan isolé.
7. **Indicateur visuel (Badge 2D)** :
   - Affichage en surbrillance d'un badge dans la barre d'outils du viewport (ex: `[ Mode 2D - Coupe X : Axe 1 (X = 0.00 m) ]`).

### 6.2 Désactivation du Mode 2D (`setMode2D(false)`)
1. **Restauration de la caméra 3D** :
   - Restauration exacte des coordonnées du point de visée (`At`), de position (`Eye`), de distance focale et de projection perspective antérieures.
2. **Restauration de la visibilité complète** :
   - Réaffichage de tous les éléments structurels du modèle selon leurs drapeaux d'affichage initiaux.

---

## 7. Synchronisation Niveau ↔ WorkPlane

Le modèle structural sépare rigoureusement la référence d'étage (`Level`) du plan de travail géométrique (`WorkPlane`) :
- **Case « Synchroniser le WorkPlane » décochée** :
  - Le niveau sélectionné devient le niveau actif (mise en valeur graphique de la trame) ; le WorkPlane 3D conserve son orientation et son altitude sans modification.
- **Case « Synchroniser le WorkPlane » cochée** :
  - Si le WorkPlane est horizontal ($\vec{n} \parallel Z$) : translation le long de $Z$ jusqu'à l'altitude du niveau (`moveToElevation`). Les axes locaux et l'origine $X/Y$ sont préservés.
  - Si le WorkPlane est vertical ($X$ ou $Y$) ou incliné : aucune translation forcée n'est appliquée, préservant l'intégrité du plan de coupe actif.

---

## 8. Commandes et Raccourcis Clavier Associés

| Commande | Identifiant `cmd.*` | Raccourci | Rôle |
| :--- | :--- | :--- | :--- |
| **Plan Horizontal XY** | `cmd.coord.workplane_xy` | `Alt+Z` | Active le plan horizontal Z (planchers) |
| **Coupe Verticale YZ** | `cmd.coord.workplane_yz` | `Alt+X` | Active la coupe verticale X (pignon) |
| **Coupe Verticale XZ** | `cmd.coord.workplane_xz` | `Alt+Y` | Active la coupe verticale Y (façade) |
| **Plan Étage Actif** | `cmd.coord.workplane_level` | — | Aligne le plan sur l'élévation du niveau actif |
| **Gestionnaire d'Étages** | `cmd.coord.manage_levels` | `Ctrl+L` | Dialogue d'édition et de création des niveaux |
| **Afficher Grille 3D** | `cmd.display.grid` | `G` *(ou `F7`)* | Bascule l'affichage de la grille spatiale |
| **Magnétisme Grille** | `cmd.snap.grid` | `S` | Active/désactive l'accrochage à la grille |
| **Accrochage Objets** | `cmd.snap.object_snap` | `F3` | Active/désactive l'accrochage OSNAP |
