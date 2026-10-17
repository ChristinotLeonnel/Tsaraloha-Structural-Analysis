# Correspondance TSA ↔ IFC

> Conçu selon IFC 4.3 (ISO 16739-1:2024), import IFC2X3 / IFC4 / IFC4X3_ADD2. Aucune certification buildingSMART
> n'est revendiquée : la conformité d'un fichier doit être vérifiée par un validateur (buildingSMART Validation
> Service, `ifcopenshell.validate`). Code : `src/BIM/IFC`. Audit et mesures : `docs/AUDIT_IO.md`.

## 1. Architecture

| Élément | Rôle |
| :--- | :--- |
| **IfcOpenShell 0.9.0post1** (LGPL-3.0, `thirdparty/`, `scripts/build_ifcopenshell.ps1`) | lecture des schémas IFC2X3, IFC4, IFC4X3_ADD2 ; noyau géométrique OpenCASCADE (toutes représentations : extrusions, B-rep, maillages, booléens, `IfcMappedItem`, ouvertures) ; relecture indépendante après export |
| `IfcOpenShellImport` (pont, bibliothèque statique isolée) | passe « objets physiques » : produits, géométrie, propriétés, quantités, matériaux, classification, étages, décomposition |
| `IfcStepReader` / `IfcImporter` | passe « modèle de calcul » : `IfcStructuralAnalysisModel` (nœuds, barres, surfaces, appuis) et liens produit ↔ calcul ; lecteur de repli si IfcOpenShell est absent (capacités réduites, signalé) |
| `IfcStepWriter`, `IfcMapper`, `IfcGeometryMapper`, `IfcPropertyMapper`, `IfcRelationshipMapper`, `IfcExporter` | export IFC4X3_ADD2 ; le modèle n'est jamais modifié par l'export |
| `IfcComparison` | comparaison de deux modèles (aller-retour, essais, `tsa-ifc roundtrip`) |
| `BimModel` / `PhysicalElement` | produit physique : GlobalId, classe d'origine, état pour le calcul, géométrie (`TriangleMesh`), Psets, remarques d'import ; persistance `.tsa` (chunks `BIMM` et `BGEO`) |

Modèle BIM et modèle de calcul sont **séparés** : un produit référence 0..N éléments de calcul (`AnalyticalRef`).
Un produit sans élément de calcul est conservé avec son état : **à préparer** (porteur : poutre, poteau, dalle,
voile, semelle… ou `LoadBearing = TRUE`) ou **non porteur** (porte, fenêtre, mobilier, proxy… ou
`LoadBearing = FALSE`).

Interface (`src/UI/MainWindow_Bim.cpp`, `src/UI/Dialogs/IfcDialogs`) :

- Importer : options (géométrie, modèle de calcul du fichier, déduction depuis la géométrie), import dans un thread
  de travail, progression et annulation, **projet importé ouvert dans un nouvel onglet** (projet sans fichier, à
  enregistrer ; les projets ouverts ne sont jamais remplacés), rapport détaillé (objets, classes, avertissements,
  enregistrement CSV).
- Exporter : écriture atomique (`QSaveFile`) d'une copie du projet dans un thread de travail, **relecture par
  IfcOpenShell** : un fichier illisible fait échouer l'export, jamais de succès annoncé sur un fichier non vérifié.
- Vue 3D : objets importés sans élément de calcul tracés avec leur géométrie d'origine (couleur du style IFC) ;
  survol et clic → propriétés IFC dans l'inspecteur (lecture seule). Bouton **« Objets IFC importés »** (Accueil >
  Projet, Affichage > Aides visuelles, menu Vue > Aides visuelles) : masque ou réaffiche la couche sans toucher au
  projet ; masquer vide l'inspecteur s'il montrait un objet IFC ; le choix est conservé d'un projet à l'autre, comme
  les autres aides visuelles, et un import réaffiche la couche.
- Ligne de commande : `tsa-ifc info | export | roundtrip` (rapport JSON).

## 2. Export (schéma `IFC4X3_ADD2`, vue `ReferenceView`)

### Structure spatiale

| TSA | IFC |
| :--- | :--- |
| Projet (couche BIM) | `IfcProject` + `IfcUnitAssignment` (m, m², m³, rad, kg, N, Pa, s, K) |
| Site / Bâtiment | `IfcSite`, `IfcBuilding` (`IfcRelAggregates`) |
| Niveau (`LevelManager`) | `IfcBuildingStorey` (attribut `Elevation`, placement à l'origine : les produits gardent les coordonnées du modèle) |
| Étage d'un produit | `IfcRelContainedInSpatialStructure` (étage imposé, sinon déduit) ; une partie d'agrégat n'est pas contenue directement (seul le tout l'est) |
| Décomposition d'origine (`TSA_Import.Parent`) | `IfcRelAggregates` tout → parties |

### Produits physiques

| Élément de calcul TSA | Produit IFC | PredefinedType |
| :--- | :--- | :--- |
| Poutre (`BarRole::Beam`, `Generic`, `SteelMember`) | `IfcBeam` | `BEAM` |
| Barre de rôle Poteau, `Column` | `IfcColumn` | `COLUMN` |
| Barre de rôle Contreventement / Tirant / Treillis / Câble | `IfcMember` | `BRACE` / `TIEBAR` / `MEMBER` / `STRUCTURALCABLE` |
| `TrussMember` | `IfcMember` | `CHORD` / `STRUT` / `BRACE` |
| `Cable` | `IfcMember` | `STAY_CABLE` / `SUSPENSION_CABLE` / `SUSPENDER` / `STRUCTURALCABLE` |
| `Slab` / `Wall` | `IfcSlab` / `IfcWall` | `FLOOR` / `SOLIDWALL` |
| `Foundation` isolée / filante / radier / pieu | `IfcFooting` / `IfcFooting` / `IfcSlab` / `IfcPile` | `PAD_FOOTING` / `STRIP_FOOTING` / `BASESLAB` / `NOTDEFINED` |

Produits importés :

- classe d'origine écrite si elle existe en IFC4X3 (attributs propres connus : portes, fenêtres, escaliers,
  assemblages, fixations…) ; `…StandardCase` / `…ElementedCase` (absentes d'IFC4X3) → classe de base ; classe
  inconnue de l'exporteur → `IfcBuildingElementProxy`. Dans ces deux derniers cas, `TSA_Import.OriginalIfcClass`
  conserve la classe d'origine, **rétablie à la relecture par TSA** ;
- géométrie : `IfcTriangulatedFaceSet` (maillage d'origine, coordonnées du projet) + `IfcStyledItem` (couleur) ;
  produit sans géométrie propre (agrégat) : sans représentation ; jamais omis ;
- nom absent conservé absent (pas de nom par défaut).

### Profils, matériaux, propriétés

- Profils paramétrés (`IfcRectangleProfileDef`, `IfcIShapeProfileDef`, `IfcUShapeProfileDef`…), `IfcMaterial` +
  `IfcMaterialProperties`, `IfcMaterialProfileSet` pour les barres.
- Psets standard (`Pset_BeamCommon`…), `Pset_TSA_Structural` (produits reliés au calcul), Psets saisis ou importés
  (prioritaires), jeu `TSA_Import` (données d'origine sans équivalent TSA : matériaux, profils, type, parent,
  représentation d'origine, ouvertures, dimensions de portes / fenêtres, classe d'origine).
- Classifications : `IfcClassification` + `IfcClassificationReference`.

### Modèle de calcul

| TSA | IFC |
| :--- | :--- |
| Modèle | `IfcStructuralAnalysisModel` (`LOADING_3D`, GlobalId persistant) + `IfcRelAssignsToGroup` |
| Nœud utilisé ou appuyé | `IfcStructuralPointConnection` (Vertex) + `IfcBoundaryNodeCondition` (6 DDL, ressorts) |
| Poutre / poteau / treillis / câble | `IfcStructuralCurveMember` `RIGID_JOINED_MEMBER` / `PIN_JOINED_MEMBER` / `CABLE` |
| Dalle / voile | `IfcStructuralSurfaceMember` `SHELL` / `BENDING_ELEMENT` / `MEMBRANE_ELEMENT` (Face, ouvertures, épaisseur) |
| Extrémités | `IfcRelConnectsStructuralMember` |
| Produit ↔ calcul | `IfcRelAssignsToProduct` (barres, surfaces ; **fondation → sa connexion d'appui**) |

## 3. Import

1. **Enveloppe ISO 10303-21** vérifiée (`ISO-10303-21;` … `END-ISO-10303-21;`) : un fichier tronqué est refusé
   (« fichier incomplet »), jamais importé en partie en silence.
2. **Unités** du projet (`IfcSIUnit` avec préfixe, `IfcConversionBasedUnit` : pouce, pied…) → m, m², m³, N, Pa ;
   la géométrie du noyau est remise à l'unité du projet si IfcOpenShell en choisit une autre (cas d'un
   `IfcProjectLibrary` sans unité).
3. **Modèle de calcul du fichier** (option, par défaut) : nœuds, barres (type, axe → rotation γ), surfaces,
   appuis, rattachés aux produits par `IfcRelAssignsToProduct`. **Liens explicites convertis** : voile relié à sa
   surface de calcul → voile TSA reconstruit depuis la géométrie du produit (hauteur, épaisseur, décalage) ;
   semelle / pieu relié à un appui → fondation sur le nœud de cet appui.
4. **Objets physiques** (IfcOpenShell) : chaque `IfcElement` (sauf ouvertures, espaces, annotations, grilles,
   éléments virtuels) → produit, fusionné par GlobalId avec celui de la passe précédente ; géométrie maillée dans le
   repère du projet (ouvertures soustraites, `IfcMappedItem` résolus) ; Psets et Psets du type, quantités, valeurs
   énumérées / listes / bornes / tables (en texte), matériaux et profils, classification, étage, parent,
   ouvertures. Produits sans géométrie : conservés (état « partiel »).
5. **Rien n'est inventé** : un produit sans élément de calcul reste « à préparer » ou « non porteur ». La
   **déduction depuis la géométrie** (axes d'extrusion → barres, contours → dalles, rectangles → voiles et
   semelles) n'a lieu que sur demande (option de la fenêtre d'import, `--derive` de `tsa-ifc`), et chaque élément
   déduit est signalé (remarque sur le produit, compteur du rapport).
6. Valeurs recalculables à l'export (Pset_TSA_Structural, LoadBearing, nom et repère par défaut, étage déduit) non
   dupliquées. Un jeu `TSA_Import` lu dans le fichier (IFC écrit par TSA) prime sur l'état du fichier, qui est une
   conversion de TSA.

### Données : conservées, converties, approchées, perdues

Mesuré par `IfcComparison` sur l'aller-retour import → export IFC4X3_ADD2 → import (test 404, `tsa-ifc roundtrip`).

| Classe | Données |
| :--- | :--- |
| **Conservées** (identiques après aller-retour) | GlobalId des produits, du projet, du site, du bâtiment, des étages et des objets de calcul ; classe d'origine ; nom, description, type d'objet, repère, type prédéfini ; Psets et quantités (valeurs) ; classifications ; étages ; décomposition ; état pour le calcul ; modèle de calcul (nœuds, barres, surfaces, fondations, appuis) |
| **Converties** | unités → SI ; représentations (extrusions, B-rep, booléens, mappings) → maillage triangulé ; `…StandardCase` → classe de base (+ classe d'origine en propriété) ; classe inconnue de l'exporteur → `IfcBuildingElementProxy` (+ classe d'origine) ; propriétés énumérées / listes / bornes / tables → texte ; liens explicites voile / semelle → éléments TSA |
| **Approchées** | géométrie : maillage du noyau OpenCASCADE (courbes facettées) ; écart d'emprise mesuré **≤ 1 µm** (9,5·10⁻⁷ m au plus sur 21 fichiers), tolérance des essais **1 mm** |
| **Perdues** (à l'export) | représentations paramétriques d'origine des objets importés (seul le maillage est réécrit), ouvertures en tant qu'objets (`IfcOpeningElement` : soustraites de la géométrie, nombre conservé), styles autres que la couleur, géoréférencement (`IfcMapConversion`), grilles (`IfcGrid`), relations non structurelles autres que contenance / décomposition / classification / matériau (connexions, remplissages, systèmes, groupes), GlobalId des relations et des Psets |

## 4. Limites connues

- Grilles `IfcGrid` : ni importées ni exportées (le `GridManager` est hors du `Model`).
- Géoréférencement (`IfcMapConversion`, `IfcSite` RefLatitude…) ignoré ; coordonnées gardées telles quelles.
- Charges, cas et combinaisons, relâchements, excentrements : non échangés.
- Export : schéma unique IFC4X3_ADD2, pas de sélection de catégories ni d'annulation (export rapide, sur copie).
- Objets importés : inspection en lecture seule (pas d'édition des Psets dans l'interface) ; conversion en éléments
  de calcul seulement par la déduction à l'import.
- Lecture et préparation des représentations d'IfcOpenShell non interruptibles : l'annulation agit entre les
  produits (délai : une géométrie par fil de calcul).
- Sans IfcOpenShell (`TSA_WITH_IFCOPENSHELL=OFF`) : lecteur interne seul, objets à représentation non paramétrique
  importés sans géométrie (signalé dans la fenêtre d'import).

## 5. Validation

- Suite `bim` (tests 170–191) : export, import d'un fichier tiers (avec et sans déduction), aller-retour du portique
  de référence, fichier écrit par IfcOpenShell, persistance des produits importés (`.tsa`, chunk `BGEO`).
- Suite `ifc` (tests 400–407), fichiers buildingSMART non modifiés (CC BY 4.0, `tests/data/ifc/buildingsmart`) :
  même bâtiment en IFC2X3 / IFC4 / IFC4X3_ADD2 (géométrie identique à 1 mm), architecture (non porteurs,
  agrégats), mur avec ouverture et fenêtre, unités en pouces et placements imbriqués, aller-retour sans perte des
  9 fichiers, fichiers invalides (tronqué, non IFC, absent), grand fichier (pont ; toiture de la grande cour,
  5 178 produits en 4,9 s avec `TSA_IFC_SAMPLES`), projet `.tsa` d'un IFC importé relu sans perte.
- Chaque export est relu par IfcOpenShell (`IfcExportReport::verifiedElements`) ; `tsa-ifc roundtrip` compare
  les deux projets et sort en erreur s'ils diffèrent.
