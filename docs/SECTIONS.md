# Sections — TSA

> Document vivant. Voir aussi `.agents/skills/add-section/SKILL.md`.

## `src/Model/Section.h` / `Section.cpp`

Système centralisé unique pour les sections transversales des éléments linéaires
(`Beam`, `Column`, `TrussMember`, `Cable`).

```cpp
enum class SectionShape
{
    Rectangular, Circular, IShape, Pipe, BoxHollow, UPN, Angle, TSection
};

struct Section
{
    int id;
    std::string name;
    SectionShape shape;
    double width, height, diameter, tw, tf; // dimensions en mètres

    double area() const;
    double iy() const; // Inertie flexion axe fort (m4)
    double iz() const; // Inertie flexion axe faible (m4)
    double it() const; // Inertie torsionnelle Saint-Venant (m4)
    double wy() const; // Module de résistance élastique fort (m3)
    double wz() const; // Module de résistance élastique faible (m3)

    static Section rectangular(double b, double h, const std::string& name = "");
    static Section circular(double d, const std::string& name = "");
    static Section ipe(int number);
    static Section hea(int number);
    static Section heb(int number);
    static Section upn(int number);
    static Section angle(double h, double b, double t, const std::string& name = "");
    static Section tSection(double h, double b, double tw, double tf, const std::string& name = "");
    static Section boxHollow(double b, double h, double tw, double tf = 0.0, const std::string& name = "");
    static Section pipe(double diameter, double thickness, const std::string& name = "");
    static std::vector<Section> defaultLibrary();
};
```

### Constante de torsion `it()` (J de Saint-Venant, transmise à `elasticBeamColumn`)

`it()` est la constante de torsion de Saint-Venant J, utilisée par le calcul (rigidité G·J,
G = E / 2(1 + ν)). Elle n'est **pas** le moment quadratique polaire Iy + Iz, sauf pour les sections
circulaires pleines ou annulaires.

| Forme | Formule | Remarque |
| :--- | :--- | :--- |
| `Rectangular` | a·b³·[1/3 − 0,21·(b/a)·(1 − (b/a)⁴/12)], a ≥ b | ≈ 0,229·a·b³ pour a/b = 2 (Timoshenko), test 321 |
| `Circular` | π·d⁴/32 | égal au moment polaire |
| `Pipe` | π·(D⁴ − d⁴)/32 | égal au moment polaire |
| `BoxHollow` | 4·Am² / ∮ds/t + Σ s·t³/3, Am = (b − tw)(h − tf) | Bredt sur la ligne moyenne, sans arrondis d'angle (EN 10219-2) ; Iy + Iz le surestimait d'environ 34 % pour un carré (test 322) |
| `IShape`, `UPN` | (2·b·tf³ + (h − tf)·tw³)/3 | parois minces ouvertes |
| `TSection` | (b·tf³ + (h − tf)·tw³)/3 | parois minces ouvertes |
| `Angle` | (h + b − t)·t³/3 | parois minces ouvertes |

Le contrôle avant calcul (`ModelValidator::validate`) refuse une poutre ou un poteau dont J est nul ou
non fini : la rotation autour de l'axe de la barre n'aurait aucune rigidité.

`TODO: VERIFY IN SOURCE` pour le détail exact des formules `iy()/iz()/wy()/wz()` dans
`Section.cpp` avant toute modification — ne pas deviner les formules existantes,
notamment pour les profils composés (`IShape`, `UPN`, `Angle`, `TSection`, `BoxHollow`).

## Cohérence UI / Model / 3D

Une section sélectionnée dans l'UI (`src/UI/Properties/PropertyPanel`) doit être
identique à celle du modèle (l'objet `Section` réel possédé par l'élément) et à celle
utilisée par le Geometry Builder correspondant (`src/Geometry/*Geometry`) pour tracer la
forme extrudée dans OCCT.

## Point de vigilance : Copy/Paste

Tester systématiquement qu'une section reste identique après copie via
`StructuralClipboard` :

```text
Circular Ø20  →  (Copy/Paste)  →  Circular Ø20 (jamais Rectangular)
```

## Bibliothèques de sections (TSALib)

Le système d'extensions dynamique `src/ExtensionSystem` (voir `docs/TSALIB_SYSTEM.md`,
document existant) permet de charger des bibliothèques de sections/matériaux
supplémentaires sans recompilation. Toute nouvelle section ajoutée au cœur de TSA
(`SectionShape`) doit rester compatible avec ce mécanisme de chargement dynamique.

## Ajouter une nouvelle forme

Voir la procédure complète dans `.agents/skills/add-section/SKILL.md`.
