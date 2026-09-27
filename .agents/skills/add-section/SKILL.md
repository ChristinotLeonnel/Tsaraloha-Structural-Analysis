---
name: add-section
description: Ajouter une nouvelle forme/famille de section (SectionShape) au système centralisé existant dans src/Model/Section.h, en garantissant la cohérence UI/Model/Geometry/3D et la robustesse au Copy/Paste.
---

# add-section

Objectif : étendre le système de sections existant (`TSA::Model::Section`,
`SectionShape`) sans créer de système parallèle, en garantissant que la section reste
identique entre UI, modèle et géométrie 3D — y compris après Copy/Paste.

## Procédure

```text
Existing Section System (src/Model/Section.h / .cpp)
→ New Section (ajout à l'enum SectionShape + usine statique dédiée)
→ Parameters (dimensions propres à la nouvelle forme)
→ Model (Section::area/iy/iz/it/wy/wz mis à jour pour la nouvelle forme)
→ Geometry (le/les Geometry Builder(s) concernés savent dessiner la nouvelle forme)
→ UI (PropertyPanel expose les bons champs de dimension pour la nouvelle forme)
→ 3D (OccView affiche correctement la section extrudée/représentée)
→ Tests (création, calculs géométriques, Copy/Paste)
```

1. Ajouter la nouvelle valeur à `enum class SectionShape` (`src/Model/Section.h`).
2. Ajouter l'usine statique correspondante (sur le modèle de `Section::ipe`,
   `Section::angle`, `Section::boxHollow`, etc.) dans `Section.cpp`.
3. Mettre à jour les formules `area()`, `iy()`, `iz()`, `it()`, `wy()`, `wz()` pour gérer
   ce nouveau cas (`switch`/`if` sur `SectionShape` — vérifier l'implémentation réelle
   dans `Section.cpp` avant d'ajouter une branche).
4. Vérifier si `Section::defaultLibrary()` doit inclure des exemplaires par défaut de la
   nouvelle forme.
5. Adapter le(s) Geometry Builder(s) concerné(s) (`src/Geometry/*Geometry`) pour tracer la
   nouvelle forme le long de l'axe de l'élément linéaire.
6. Adapter `PropertyPanel` pour exposer les champs pertinents à la nouvelle forme (et
   masquer les champs non pertinents des autres formes).
7. Vérifier que `TSALib`/`src/ExtensionSystem` (si les sections sont exposées comme
   bibliothèques chargeables à chaud) reste cohérent avec le nouvel enum.

## Test critique : Copy/Paste

Tester particulièrement que la section est préservée après copie via
`StructuralClipboard` :

```text
Circular Ø20
```

ne doit **jamais** devenir :

```text
Rectangular
```

après Copy/Paste. Ajouter un test explicite dans `tests/` reproduisant ce scénario pour
la nouvelle forme si un test générique n'existe pas déjà.

## Vérification

`TODO: VERIFY IN SOURCE` pour la structure exacte des formules `iy()/iz()/it()/wy()/wz()`
dans `Section.cpp` avant d'ajouter une nouvelle forme — ne pas deviner les formules
existantes.
