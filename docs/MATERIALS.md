# Matériaux — TSA

> Document vivant. Voir aussi `src/Model/Material.h`, `src/Model/MaterialLibrary.h`.

## `MaterialType` (`src/Model/Material.h`)

```cpp
enum class MaterialType
{
    Concrete = 0, Steel = 1, Timber = 2, Masonry = 3, Custom = 4,
    ReinforcedConcrete = 5, RebarSteel = 6, GalvanizedSteel = 7, Aluminum = 8,
    Brick = 9, Glass = 10, Soil = 11, Sand = 12, Gravel = 13, Rock = 14
};
```

## `MechanicalProperties`

Propriétés physiques/mécaniques intrinsèques, destinées aux calculs EF/analyse
structurale — **c'est la partie « métier »** du matériau, source de vérité pour tout
calcul :

```cpp
struct MechanicalProperties
{
    double youngModulus = 31.0e9;  // E en Pa
    double poissonRatio = 0.20;    // nu
    double density = 2500.0;       // rho en kg/m³
    double yieldStrength = 25.0e6; // fk/fck/fy en Pa
    double thermalCoeff = 1.0e-5;  // dilatation thermique 1/K

    double E() const; double nu() const; double rho() const; double fk() const;
};
```

## `VisualProperties`

Propriétés visuelles et de rendu réaliste dans le viewport, destinées à OCCT PBR/ombrage
— **strictement séparées** des propriétés mécaniques ; ne jamais utiliser une valeur
visuelle comme donnée de calcul ni l'inverse :

```cpp
struct VisualProperties
{
    std::string baseColor = "#A0A0A0";
    double roughness = 0.85;
    double metallic = 0.0;
    double transparency = 0.0;
    double shininess = 0.10;
    std::string textureName;
    std::string texturePath;
    double textureScaleU = 1.0;
    double textureScaleV = 1.0;
};
```

`TODO: VERIFY IN SOURCE` pour la structure complète de `Material` reliant `MaterialType`,
`MechanicalProperties` et `VisualProperties`, et pour l'API de `MaterialLibrary`
(`src/Model/MaterialLibrary.h/.cpp`) — probable équivalent de `Section::defaultLibrary()`
pour les matériaux.

## Rendu visuel (`src/Viewer/MaterialVisual`, `TextureManager`)

Ces classes du viewer consomment `VisualProperties` (et éventuellement `TextureManager`
pour la résolution effective des textures) pour l'affichage OCCT — elles ne redéfinissent
jamais elles-mêmes une apparence indépendante du `Material` du modèle.

## Bibliothèques de matériaux (TSALib)

Comme pour les sections, le système `src/ExtensionSystem` (voir `docs/TSALIB_SYSTEM.md`)
permet de charger des bibliothèques de matériaux supplémentaires sans recompilation.
