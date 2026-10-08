# Architecture BIM de TSA

Document de référence de la migration openBIM (démarrée le 2026-10-06). Il contient le rapport
préalable (architecture actuelle, écarts, plan) puis l'architecture cible et l'état d'avancement.
Formulations : TSA est **conçu selon les principes** d'ISO 19650 / openBIM et **compatible avec**
IFC 4.3 (ISO 16739-1:2024) dans le périmètre décrit ; aucune conformité ni certification n'est
revendiquée sans validation externe.

---

## 1. CURRENT ARCHITECTURE (constat du 2026-10-06)

| Domaine | Existant | Fichiers |
| :--- | :--- | :--- |
| Modèle | `TSA::Model::Model`, source de vérité unique, observateurs, snapshots (Undo/Redo), révision | src/Model/Model*.cpp |
| Nœuds | `Node` (id int, x/y/z, levelId, appui 6 DDL `SupportDefinition`) | Node.h |
| Barres | `Beam` (rôle `BarRole`, section et matériau **copiés par valeur**, rotation β, excentrement, relâchements), `Column`, `TrussMember`, `Cable` ; ids int **par famille** | Beam.h, Column.h… |
| Surfaces / fondations | `Slab` (contour de nœuds, épaisseur), `Wall` (2 nœuds, hauteur), `Foundation` | |
| Matériaux | `Material` (nom, `MaterialType`, propriétés mécaniques E, ν, ρ, fk, α, visuel), bibliothèque | Material.h |
| Sections | `Section` (forme, dimensions, A, Iy, Iz, It, Wy, Wz, catalogue IPE/HEA/HEB/UPN…) | Section.h |
| Charges | `LoadManager` : `LoadCase` (catégorie EN 1990), `LoadCombination` (coefficients), `NodalLoad`, `MemberLoad` (uniforme, trapézoïdale, ponctuelle, moment, poids propre) — déjà des **données du modèle** | src/Model/Load |
| Niveaux / plans | `LevelManager` (id, nom, cote), `WorkPlaneManager`, `CoordinateSystem` | src/Coordinate |
| Grilles | `GridManager` (axes 1,2,3 / A,B,C, libellés personnalisables, origine, rotation) — **hors du Model** (MainWindow) | src/Grid |
| Calcul | Multi-moteurs (ADR-017) : `AnalysisManager`, `AnalysisModel` dérivé (snapshot + mapping), OpenSees (3D), Custom2D / MetDeDeplacement (2D) | src/Analysis |
| Résultats | `ResultsModel` (un seul calcul, en mémoire, non persisté), métadonnées d'exécution | ResultsModel.h |
| Historique | `UndoManager` + `EditTransaction` + `EditRecord` (avant / après / impacts) | src/UndoRedo |
| Fichier | `.tsa` binaire à chunks FourCC (1.2), chunks inconnus ignorés | src/IO |
| IA | `AIToolRegistry` : outils de lecture + `propose_*` validés par l'ingénieur | src/AI |
| Graphique | `OccView` (OCCT) ; géométrie construite à partir du modèle, jamais l'inverse | src/Viewer, src/Geometry |

## 2. BIM GAP ANALYSIS

| Exigence | Existant | Écart |
| :--- | :--- | :--- |
| Identité stable / GlobalId IFC | ids int par famille, recyclables | **Manquant** : aucun GUID, pas d'objet « produit » |
| Physique ≠ analytique | une barre TSA est à la fois l'objet physique et l'élément de calcul | **Manquant** : pas de produit physique, pas de mapping 1:N (une division crée une barre indépendante) |
| Hiérarchie spatiale | niveaux (cotes) ; nœuds rattachés à un niveau | **Partiel** : pas de Projet / Site / Bâtiment, éléments non rattachés à un étage |
| Grilles | complètes, mais hors Model | Position « B-3 » non calculée |
| Matériaux | riches mais copiés par élément | **Partiel** : pas de définition partagée référencée |
| Profils | `Section` par valeur | **Partiel** : pas de séparation définition / instance |
| Charges / combinaisons | données du modèle | Conforme à l'exigence ; pas d'identifiant global |
| Résultats | un calcul, non persisté | **Manquant** : `AnalysisRun`, historique, audit, comparaison |
| Versionnage | Undo + EditRecord | **Partiel** : pas de comparaison de versions par objet |
| IFC | aucun | **Manquant** : export, import, mapping |
| Propriétés BIM (Pset) | aucune | **Manquant** |
| IDS / BCF | aucun | **Manquant** |
| Unités | conversions locales (Pa→kPa, kN…) | **Manquant** : module centralisé |
| API IA | outils de lecture / proposition | **Partiel** : pas de requêtes BIM (étage, catégorie, profil) |

## 3. MIGRATION PLAN

Règle : **fonctionnalités TSA existantes + capacités BIM**, jamais l'inverse. Les éléments TSA
actuels (nœuds, barres, dalles…) **deviennent le modèle analytique** sans changement ; le modèle
BIM est une couche ajoutée **dans** le `Model` (même snapshot, même Annuler, même fichier).

| Phase | Contenu | Statut |
| :--- | :--- | :--- |
| 1 | `TSA::BIM::BimModel` : produits physiques, catégories IFC, Psets | FAIT (2026-10-06, tests 170, 175) |
| 2 | Physique / analytique : `PhysicalElement` → N éléments analytiques ; divisions rattachées au même produit | FAIT (test 173) |
| 3 | Identifiants : id interne de produit + GlobalId IFC (22 car.) distincts, persistés, conservés par Annuler | FAIT (tests 170–176) |
| 4 | Projet / Site / Bâtiment / Étages (← niveaux), étage de chaque produit, position de grille « B-3 » | PARTIEL : structure spatiale et étages faits (test 174) ; repère de grille à faire |
| 5 | Catalogues de matériaux et de profils (définitions partagées, références par nom) | |
| 6 | Charges : déjà des données ; identifiants et export structurel IFC | |
| 7 | `AnalysisRun` / historique / audit | |
| 8 | Export IFC 4.3 (couche de mapping, validation IfcOpenShell) | FAIT (test 177 ; IfcOpenShell 0.9 : 0 erreur de schéma / règles EXPRESS) |
| 9 | Import IFC + aller-retour | FAIT (tests 178–180 ; fichier IfcOpenShell en mm) |
| 10 | Validation BIM avant calcul | |
| 11 | IDS (sous-ensemble) | |
| 12 | BCF (abstraction) | |
| 13 | API contrôlée pour l'IA | |

---

## 4. ARCHITECTURE CIBLE

```text
                         TSA::Model::Model  (source de vérité, snapshots, Annuler)
          ┌──────────────────────┴──────────────────────┐
   BIM MODEL (TSA::BIM::BimModel)                ANALYTICAL MODEL (éléments TSA existants)
   Projet / Site / Bâtiment / Étages             Nœuds, barres, dalles, voiles, appuis,
   PhysicalElement (GlobalId, catégorie IFC,      relâchements, charges
   étage, Psets, classification)                        │
          │        PhysicalToAnalyticalMap (1 → N)      ▼
          └────────────────────────────────────▶  AnalysisManager → OpenSees | Custom2D
                                                        │
                                                        ▼
                                            AnalysisRun / ResultsModel (par calcul)
          │                                             │
          ▼                                             ▼
   IFC (export / import / mapping)      NDC, visualisation, API IA (requêtes, propositions)
```

Correspondance avec l'arborescence demandée (le dépôt garde sa structure `src/<module>`) :

| Demandé | TSA |
| :--- | :--- |
| /core/bim | src/BIM/Core |
| /core/geometry, /core/model | src/Geometry, src/Model (inchangés) |
| /core/materials, /core/profiles | src/Model (Material, Section) + src/BIM/Core/Catalogs |
| /core/loads, /core/results | src/Model/Load, src/Analysis (Results, AnalysisRun) |
| /ifc/import, /ifc/export, /ifc/mapping | src/BIM/IFC |
| /solver | src/Analysis/Engines + TSALab/science (cœur scientifique) |
| /validation/bim, /ids, /model | src/BIM/Validation, src/Standards/ModelValidator, src/Model/ModelCleanup |
| /ui | src/UI |
| unités | src/Core/Units.h |

Détails par sujet : `IFC_MAPPING.md`, `ANALYTICAL_MODEL.md`, `BIM_GUIDELINES.md`, `AI_API.md`,
`VERSIONING.md`.
