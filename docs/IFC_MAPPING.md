# Correspondance TSA ↔ IFC

> Implémentation partielle, conçue selon IFC 4.3 (ISO 16739-1:2024). Aucune certification
> buildingSMART n'est revendiquée : la conformité d'un fichier doit être vérifiée par un validateur
> (IfcOpenShell, buildingSMART Validation Service). Code : `src/BIM/IFC`.

## 1. Architecture

| Classe | Rôle |
| :--- | :--- |
| `IfcStepWriter` / `IfcStepReader` | Écriture / lecture STEP ISO 10303-21 (échappement `''`, Unicode `\X2\…\X0\`, réels `1.E-05`), sans dépendance au rendu |
| `IfcMapper` | Entités et types prédéfinis, profils paramétrés, matériaux (+ propriétés), jeux de profils |
| `IfcGeometryMapper` | Placements, solides extrudés (SweptSolid), axe (Curve3D) ; repère des barres identique au viewport |
| `IfcPropertyMapper` | Psets standard, `Pset_TSA_Structural`, Psets utilisateur (prioritaires) |
| `IfcRelationshipMapper` | Relations `IfcRel*` regroupées, écrites en fin d'export |
| `IfcExporter` / `IfcImporter` | Orchestration ; le modèle n'est jamais modifié par l'export |

Interface : Fichier > Importer IFC… / Exporter IFC… et ruban Accueil > Projet (`src/UI/MainWindow_Bim.cpp`,
câblage seulement).

## 2. Export (schéma `IFC4X3_ADD2`, vue `ReferenceView`)

### Structure spatiale

| TSA | IFC |
| :--- | :--- |
| Projet (couche BIM) | `IfcProject` + `IfcUnitAssignment` (m, m², m³, rad, kg, N, Pa, s, K) |
| Site / Bâtiment | `IfcSite`, `IfcBuilding` (`IfcRelAggregates`) |
| Niveau (`LevelManager`) | `IfcBuildingStorey` (attribut `Elevation`, placement à l'origine : les produits gardent les coordonnées du modèle) |
| Étage d'un produit | `IfcRelContainedInSpatialStructure` (étage imposé, sinon déduit du nœud le plus bas) |

### Produits physiques

| Élément analytique TSA | Produit IFC | PredefinedType |
| :--- | :--- | :--- |
| Poutre (`BarRole::Beam`, `Generic`, `SteelMember`) | `IfcBeam` | `BEAM` |
| Barre de rôle Poteau, `Column` | `IfcColumn` | `COLUMN` |
| Barre de rôle Contreventement / Tirant / Treillis / Câble | `IfcMember` | `BRACE` / `TIEBAR` / `MEMBER` / `STRUCTURALCABLE` |
| `TrussMember` (membrure / montant-diagonale / contreventement) | `IfcMember` | `CHORD` / `STRUT` / `BRACE` |
| `Cable` (hauban, porteur, suspente, autre) | `IfcMember` | `STAY_CABLE` / `SUSPENSION_CABLE` / `SUSPENDER` / `STRUCTURALCABLE` |
| `Slab` | `IfcSlab` | `FLOOR` |
| `Wall` | `IfcWall` | `SOLIDWALL` |
| `Foundation` isolée / filante / radier / pieu | `IfcFooting` / `IfcFooting` / `IfcSlab` / `IfcPile` | `PAD_FOOTING` / `STRIP_FOOTING` / `BASESLAB` / `NOTDEFINED` |

Un produit divisé en N barres = **un** produit IFC avec N solides (`IfcExtrudedAreaSolid`) et un axe
polyligne. Le type prédéfini saisi est conservé s'il appartient à l'énumération IFC 4.3, sinon `NOTDEFINED`.

### Profils et matériaux

| `SectionShape` | Profil IFC |
| :--- | :--- |
| Rectangular / Circular / Pipe | `IfcRectangleProfileDef` / `IfcCircleProfileDef` / `IfcCircleHollowProfileDef` |
| IShape / BoxHollow | `IfcIShapeProfileDef` / `IfcRectangleHollowProfileDef` |
| UPN / Angle / TSection | `IfcUShapeProfileDef` / `IfcLShapeProfileDef` / `IfcTShapeProfileDef` |

Matériau : `IfcMaterial` (catégorie `concrete`, `steel`, `wood`…) + `IfcMaterialProperties`
`Pset_MaterialMechanical` (YoungModulus, PoissonRatio, ThermalExpansionCoefficient),
`Pset_MaterialCommon` (MassDensity), `Pset_TSA_Material` (CharacteristicStrength).
Éléments linéaires : `IfcMaterialProfileSet` (matériau + profil) ; autres : `IfcMaterial`.

### Propriétés

- Standard : `Pset_BeamCommon`, `Pset_ColumnCommon`, `Pset_MemberCommon`, `Pset_SlabCommon`,
  `Pset_WallCommon` (LoadBearing ; Span pour les poutres).
- `Pset_TSA_Structural` : InternalId, AnalyticalElements (« B1,B2 »), SectionName, MaterialName,
  MemberLength, RotationAngleDeg, Thickness, Height, dimensions de fondation, SoilBearingCapacity (Pa),
  InitialTension (N).
- Psets saisis dans la couche BIM : ajoutés, prioritaires à nom égal.
- Classifications : `IfcClassification` + `IfcClassificationReference`.

### Modèle analytique

| TSA | IFC |
| :--- | :--- |
| Modèle | `IfcStructuralAnalysisModel` (`LOADING_3D`, GlobalId persistant) + `IfcRelAssignsToGroup` |
| Nœud utilisé ou appuyé | `IfcStructuralPointConnection` (topologie Vertex) |
| Appui (6 DDL) | `IfcBoundaryNodeCondition` : `IFCBOOLEAN(.T./.F.)`, ressort `IFCLINEARSTIFFNESSMEASURE` / `IFCROTATIONALSTIFFNESSMEASURE` |
| Poutre / poteau | `IfcStructuralCurveMember` `RIGID_JOINED_MEMBER` (Axis = axe « hauteur » du profil) |
| Treillis / câble | `IfcStructuralCurveMember` `PIN_JOINED_MEMBER` / `CABLE` |
| Dalle / voile | `IfcStructuralSurfaceMember` `SHELL` (topologie Face, épaisseur) |
| Extrémités | `IfcRelConnectsStructuralMember` |
| PhysicalToAnalyticalMap | `IfcRelAssignsToProduct` (objets analytiques → produit physique) |

## 3. Import

1. Unités du fichier (`IfcSIUnit` avec préfixe, `IfcConversionBasedUnit`) → mètres et pascals.
2. Modèle analytique présent : nœuds (Vertex), barres (Edge, type, Axis → rotation γ), dalles
   (Face), appuis (`IfcBoundaryNodeCondition`), rattachés aux produits par `IfcRelAssignsToProduct`.
3. Sinon, déduction depuis la géométrie physique : axe (`Axis`) ou extrusion (`IfcExtrudedAreaSolid`)
   pour les barres ; contour (`IfcPolyline`, `IfcIndexedPolyCurve`) pour les dalles (nœuds = face
   supérieure) ; rectangle (paramétré ou contour à 4 côtés) pour les voiles (axe, épaisseur, décalage)
   et les semelles (nœud en tête) ; cercle pour les pieux.
4. Conservés : GlobalId (produits, objets analytiques, projet, site, bâtiment, étages, modèle
   analytique), noms, descriptions, repères, types prédéfinis, Psets saisis, classifications,
   matériaux, profils. Les valeurs recalculables (Pset_TSA_Structural, LoadBearing, Span, nom et
   repère par défaut, étage déduit) ne sont pas dupliquées.

## 4. Limites connues (implémentation partielle)

- Charges, cas et combinaisons : non exportés en IFC (`IfcStructuralLoadGroup`… à faire).
- Relâchements d'extrémité, excentrements, raideurs d'appui orientées : non exportés.
- Unités dérivées (module d'élasticité, masse volumique) non déclarées : valeurs SI implicites.
- GlobalId des relations et des Psets recréés à chaque export (seuls les objets ont une identité).
- Fondations : pas d'objet analytique IFC propre (le nœud d'appui l'est) ; leur GlobalId analytique
  n'est pas transmis.
- Import : représentations `IfcMappedItem`, `IfcBooleanResult`, B-rep, arcs (`IfcArcIndex`) et
  contours de voile non rectangulaires signalés et ignorés ; barres courbes non prises en charge.
- Grilles (`IfcGrid`) non exportées (le `GridManager` est hors du `Model`).

## 5. Validation

- Tests 177–180 (suite `bim`) : export, import d'un fichier tiers, aller-retour complet (GlobalId,
  géométrie à 1e-9 m, profils, matériaux, rotation, niveaux, mapping, Psets, ré-export identique),
  import d'un fichier écrit par IfcOpenShell 0.9 (`tests/data/ifcopenshell_fixture.ifc`, unités mm).
- Contrôle externe (outil de développement, hors dépendances TSA) : IfcOpenShell 0.9
  `ifcopenshell.validate` (schéma + règles EXPRESS) — 0 anomalie sur le portique de test ; noyau
  géométrique IfcOpenShell : 13/13 produits générés.
