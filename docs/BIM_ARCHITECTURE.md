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

---

## 5. CRÉATION DES ÉLÉMENTS DESSINÉS ET QUANTITÉS (2026-10-17)

Audit : `BIM_DRAWING_AUDIT.md` ; feuille de route : `BIM_DRAWING_ROADMAP.md` ; bilan :
`BIM_DRAWING_IMPLEMENTATION_REPORT.md`.

### 5.1 Séparation des responsabilités

```text
 Vue 3D (OccView, modes Dessin)      Dialogues Barre / Câble / Surface      Outils ModelingTool
   clic → SnapEngine → DrawPoint       saisie → DrawPoint                     apply() en transaction
            │                                   │
            └──────────────┬────────────────────┘
                           ▼
            TSA::Interaction::ElementCreation   (sans vue, testé sans fenêtre)
              1. validation complète (géométrie, section, matériau, doublon)  → refus motivé, modèle intact
              2. EditTransaction : nœuds manquants + élément + réglages du dialogue → UNE entrée Annuler
              3. avertissements : extrémité posée sur une barre sans nœud, croisement sans nœud commun
                           ▼
            TSA::Model::Model ── synchronisation ──▶ BimModel (produit physique, GlobalId, classe IFC, étage)
                           │
                           ├──▶ ResultsValidityGuard : résultats de calcul marqués périmés
                           └──▶ QuantityTakeoff (métré dérivé) ──▶ palette « Identité BIM et quantités »
```

La vue ne contient plus de logique de création : elle mémorise des **points de tracé**
(`DrawPoint` : position accrochée + nœud visé, −1 pour un point libre), affiche l'aperçu, puis
appelle le service et publie son compte rendu (`OccView::reportCreation` : succès et réserves,
ou « Non créé : motif »).

### 5.2 Règles de création

| Règle | Mise en œuvre |
| :--- | :--- |
| Aucun objet invalide | validations avant toute modification ; un refus ne touche ni au modèle, ni à l'historique, ni à la révision (résultats toujours valides) |
| Aucun nœud orphelin | nœuds créés à la validation de l'élément, dans sa transaction ; Échap pendant un tracé ne laisse rien |
| Annuler complet | une création (nœuds + élément + produit BIM + réglages du dialogue) = une entrée Annuler |
| Coïncidence ≠ proximité | un point confondu avec un nœud (`GeometryTolerance::nodeCoincidence` = 1 mm) le réutilise ; au-delà, nouveau nœud, jamais de fusion. Viser un nœud : l'accrochage (`SnapEngine`) |
| Croisement ≠ connexion | une extrémité sur l'intérieur d'une barre, ou un croisement sans nœud commun, est signalé ; aucune division automatique (outils Diviser / Intersecter) |
| Aucune valeur inventée | dimensions et sections des préréglages explicites (poutre, poteau, dalle, voile, **fondation**, **treillis**), modifiables dans « Préréglages de structure » ; plus de poteau de 3 m créé par un double clic ; matériaux proposés limités à ceux de la bibliothèque |
| Doublons | même famille sur les mêmes nœuds (dans les deux sens), dalle sur le même contour, deuxième fondation sous un nœud : refusés |
| Dalle | ≥ 3 points distincts, non alignés, plans (1 mm), contour simple (aucun côté ne recoupe un autre) |
| Voile | base inclinée (pieds à plus de 1 mm de cote l'un de l'autre) acceptée et signalée : élévation en parallélogramme, longueur en plan × hauteur, même géométrie pour l'affichage, le calcul (`wallSurface` : repère U horizontal, V vertical), le métré et l'export IFC (profil en parallélogramme extrudé dans l'épaisseur, relu par le lecteur interne et par IfcOpenShell). Porte au pied refusée sur une base inclinée (ouvertures intérieures seules) |

### 5.3 Quantités

`TSA::BIM::QuantityTakeoff` (`src/BIM/Core`) calcule à chaque appel, depuis le modèle seul :
longueur d'axe, surface brute, ouvertures déduites, surface nette, volume, masse (volume × masse
volumique du matériau), par élément analytique, puis cumulés par produit physique, matériau,
section ou épaisseur, et famille. Aucune quantité n'est stockée : elles suivent les modifications
et l'historique. Un produit importé sans élément de calcul n'a pas de métré (sa géométrie IFC n'est
pas mesurée).

### 5.4 Palette de propriétés

`ElementBimPanel`, sous la vue de l'élément sélectionné (poutre, poteau, câble, treillis, dalle,
voile, fondation), en lecture seule : nom et numéro du produit, classe IFC et type prédéfini,
GlobalId (sélectionnable), étage, éléments de calcul regroupés, GlobalId analytique, quantités de
l'élément et du produit entier, type de produit et écart éventuel. Rafraîchie à chaque modification du modèle et après
Annuler.

### 5.5 Navigateur BIM

Branche « Structure BIM (IFC) » de l'arbre du modèle : site › bâtiment › un étage par niveau (cote croissante) ›
produits physiques, classés par `BimModel::resolvedStorey` (« Sans étage » à part), classe IFC en colonne. Branche
dérivée du modèle, reconstruite une fois par rafale de modifications. Un clic sur un produit sélectionne son premier
élément de calcul ; un objet IFC importé sans calcul ouvre l'inspecteur IFC.

### 5.6 Types de produits partagés

`ProductType` (dans le `BimModel`) : catégorie, nom, GlobalId, section et matériau (éléments linéaires), épaisseur et
matériau (dalles, voiles), matériau (fondations). Un produit y est rattaché par `typeId`. Persistance : champs
facultatifs du bloc BIM (`types`, `type`), schéma 1 inchangé, donc un fichier existant se lit tel quel (produits sans
type) et une version antérieure de TSA ignore les types sans rejeter le bloc.

| Opération | Effet (une transaction, une entrée Annuler) |
| :--- | :--- |
| Créer les types depuis le modèle | un type par définition distincte, produits sans type rattachés (migration d'un projet existant) |
| Propager la définition d'un élément au type | le type reprend la section, l'épaisseur et le matériau de l'occurrence, puis les applique aux autres |
| Réappliquer le type | ramène les occurrences modifiées à la définition du type |
| Écart | occurrence différente de son type (« section R30x60 au lieu de R30x50 »), affiché dans la palette et l'arbre |

IFC : `IfcBeamType`, `IfcColumnType`, `IfcMemberType`, `IfcSlabType`, `IfcWallType`, `IfcFootingType`, `IfcPileType`,
`IfcPlateType` reliés par `IfcRelDefinesByType` ; à l'import, types recréés avec leur GlobalId et définition reprise
de la première occurrence.
