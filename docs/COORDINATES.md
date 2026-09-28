# Système de Coordonnées — TSA

> Document vivant. Voir aussi `DOCUMENTATION.md` section 5 (Grilles 3D et Niveaux
> d'Étage, présentation utilisateur) — ce document couvre l'architecture technique.

## `src/Coordinate/Point3D`

`class Point3D { double x, y, z; }` — convertible implicitement vers/depuis `gp_Pnt`
(OpenCASCADE) via un constructeur et un opérateur de conversion (`operator gp_Pnt()`,
`toGpPnt()`). Fournit `distance(other)` et d'autres utilitaires géométriques.
`TODO: VERIFY IN SOURCE` pour la liste complète des méthodes au-delà de `distance`.

## `src/Coordinate/Level` et `LevelManager`

- `struct Level { std::string id, name; double elevation; bool visible; }` — un étage.
- `class LevelManager : public QObject` — gestionnaire de la liste des `Level` :
  `addLevel`, `addLevelWithId`, `removeLevel`, `getLevel` (par id), `getLevelByIndex`,
  `getLevelIndex`, `setLevelElevation`, `setLevelName`, `setLevelVisible`,
  `levels()`/`levelCount()`/`isEmpty()`. `QObject` : expose vraisemblablement des
  signals Qt sur ajout/suppression/modification — `TODO: VERIFY IN SOURCE`.

## `src/Coordinate/CoordinateSystem`

`class CoordinateSystem : public QObject` — grille de coordonnées X/Y/Z de l'ensemble du
modèle :

- X : `xPositions()`, `setXPositions`, `setXSpacings`/`getXSpacings`.
- Y : `yPositions()`, `setYPositions`, `setYSpacings`/`getYSpacings`.
- Z : géré **de manière centrale via `LevelManager`** — `zLevels()`, `zCount()`,
  `setZLevels`, `setZSpacings`/`getZSpacings` délèguent aux niveaux (`Level::elevation`)
  plutôt que de maintenir une liste Z indépendante.

C'est la source de vérité des positions de grille utilisées par `src/Grid`
(`GridManager`, `GridSnapManager`, `GridRenderer`) pour le rendu et l'accrochage.

## `src/Coordinate/CylindricalCoordinates`

Support de coordonnées cylindriques (complément aux grilles cartésiennes X/Y/Z).
`TODO: VERIFY IN SOURCE` pour l'API exacte.

## Principe

Toute position dans TSA (nœud, niveau, grille) doit se référer à ce système central
(`CoordinateSystem`/`LevelManager`/`Point3D`) plutôt qu'à des coordonnées ad-hoc stockées
localement dans un widget UI ou un builder de géométrie.

## Synchronisation Niveau ↔ WorkPlane

Un niveau (`Level`) est une référence structurelle ; un `WorkPlane` est un plan de travail
géométrique. Ils ne sont **pas** fusionnés. La relation est contrôlée par l'option
`OccView::syncWorkPlaneWithLevel` (case « Synchroniser le WorkPlane » du sélecteur de niveau).

Règle déterministe (`WorkPlane::moveToElevation`) appliquée quand un niveau est sélectionné :

1. Option décochée : le niveau devient le niveau actif (surbrillance de grille) ; le WorkPlane n'est pas modifié.
2. Option cochée et WorkPlane **horizontal** (normale ∥ Z, y compris axes X/Y tournés) : translation
   le long de Z jusqu'à l'altitude du niveau. Origine X/Y et axes locaux conservés.
3. Option cochée et WorkPlane vertical, incliné, 3 points ou arbitraire : **aucune modification**
   (le plan reste indépendant du niveau).

Seule la transformation du WorkPlane change : jamais les coordonnées globales du modèle structural.
La mise à jour visuelle se fait par `applyWorkPlaneTransformation()` (transformation locale des objets AIS
existants, sans reconstruction de la grille ni du modèle).
