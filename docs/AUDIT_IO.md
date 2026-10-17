# Audit des entrées / sorties de TSA et TSALab — 17/10/2026

Audit préalable à la mission « Rendre les entrées/sorties de TSA et TSALab entièrement opérationnelles — priorité
IFC ». Chaque constat renvoie au code lu ou à une mesure. État initial mesuré avec `tsa-ifc info`
(`sdk/tools/tsa-ifc`, ajouté pour cet audit) sur des fichiers réels.

## 1. Inventaire des formats

| Format | Sens | Code | État constaté | Tests |
| :--- | :--- | :--- | :--- | :--- |
| `.tsa` (natif, chunks FourCC) | ouvrir / enregistrer | `src/IO/TSAFile*` | opérationnel ; fichier 1.0 relu, version majeure future refusée | `file_io` (test 91…) |
| `.tsalab` (TSALab) | ouvrir / enregistrer | `TSALab/lab/LabApp` | opérationnel | TSALab L4–L30 |
| `.tsa` → TSALab | import | `TSALab/lab/Research/Structural/TsaImport` | opérationnel | TSALab |
| IFC (STEP) | import / export | `src/BIM/IFC/*` (parseur et écrivain maison) | **export** : valide sur le modèle TSA (IfcOpenShell 0.9 : 0 erreur de schéma) ; **import : inopérant sur les fichiers BIM réels** (§ 2) | `bim` 177–180, 1 fichier synthétique de 4 Ko |
| TSA3D (JSON) | import / export | `src/IO/Tsa3d/*` | opérationnel, validateur `tsa3d-validate` | `tsa3d` |
| Modules (conversion → TSA3D) | import / export | `ModulesDialog`, `sdk/modules` | opérationnel (exemple CSV) | `modules` |
| Maillages (msh, vtk, unv, inp, bdf, mesh) | import / export | `MainWindow_Mesh.cpp`, Gmsh | opérationnel (formats de Gmsh) | `mesh` |
| CSV / JSON / TXT (résultats, topologie) | export | `AnalysisDataDock`, `TopologyDialog` | opérationnel | — |
| Tcl OpenSees, notes de calcul PDF / HTML, Blueprint | export / divers | — | hors périmètre de l'échange de modèles | — |
| DXF | — | **absent** | aucun code, aucune commande | — |

TSALab n'a pas d'échange IFC propre : il importe les projets TSA.

## 2. IFC — état initial mesuré

Fichiers : jeux de certification buildingSMART (CC BY 4.0, `buildingSMART/Certification-datasets`) et fichiers
publics d'essai d'IfcOpenShell (`IfcOpenShell/files`, licence non déclarée : essais locaux seulement).

| Fichier | Schéma | Produits porteurs dans le fichier | Importés | Cause du rejet |
| :--- | :--- | ---: | ---: | :--- |
| Building-Structural (buildingSMART) | IFC2X3 / IFC4 / IFC4X3_ADD2 | 11 | **0** | `IfcFacetedBrep` / `IfcTriangulatedFaceSet` non lus |
| Building-Architecture (buildingSMART) | les trois | 7 | **0** | idem |
| Infra-Bridge (buildingSMART) | les trois | 37 | **0** | idem |
| wall-with-opening-and-window (buildingSMART, RV 1.2) | IFC4 | 1 | 1 | — (fenêtre et ouverture non lues) |
| revit2011_wall1, revit2014_multiple_bounded_halfspaces | IFC2X3 | 1 | **0** | `IfcBooleanClippingResult` |
| nested_mapped_item | IFC2X3 | 24 | **0** | `IfcMappedItem` |
| acad2010_walls | IFC2X3 | 8 | **0** | représentation non paramétrique |
| geometrygym_great_court_roof (3,7 Mo) | IFC2X3 | 5 178 | 5 178 | — (barres à axe simple) |
| standard_case_element_beam | IFC4 | 18 | 18 | — |
| invalid.ifc | — | — | refusé | correct |

Dans tous les cas de rejet, **l'import se déclare réussi** (`ok`) avec zéro élément.

## 3. Problèmes identifiés

| N° | Gravité | Problème | Où |
| :--- | :--- | :--- | :--- |
| IO1 | critique | Import IFC inopérant sur les fichiers BIM réels : seules les extrusions paramétriques (`IfcExtrudedAreaSolid`) et les axes en polyligne sont lus ; maillages, B-rep, booléens, `IfcMappedItem` rejetés | `IfcImporter::representation` |
| IO2 | critique | Succès annoncé sans aucun élément importé | `IfcImporter::run`, `MainWindow::onActionImportIfc` |
| IO3 | critique | Le projet ouvert est remplacé **avant** la lecture : un fichier invalide fait perdre le projet courant | `MainWindow::onActionImportIfc` (`onActionNew()` d'abord) |
| IO4 | haute | Produit sans équivalent analytique **jeté** ; classes non porteuses (portes, fenêtres, escaliers, toitures, ouvertures, `IfcBuildingElementProxy`) jamais lues ; le modèle BIM supprime les produits sans élément analytique | `importProducts`, `BimModel::synchronize` |
| IO5 | haute | Modèle analytique **inventé** sans avertissement : barres créées depuis les axes d'extrusion (nœuds non reliés quand une poutre s'arrête au nu d'un poteau) | `IfcImporter::fromGeometry` |
| IO6 | haute | Propriétés inventées : matériau par défaut acier S235 ou béton C25/30 (aussi quand la catégorie IFC manque, cas de tout IFC2X3), section 300×500 | `material()`, `fromGeometry` |
| IO7 | haute | Parseur STEP minimal : instances complexes refusées (tout le fichier échoue), attributs lus par position sans connaissance du schéma (IFC2X3 ≠ IFC4), mémoire élevée (copie du fichier, chaînes par entité) | `IfcStepReader` |
| IO8 | moyenne | Interface synchrone : pas de progression, pas d'annulation, pas de rapport détaillé (objets importés / ignorés / partiels) | `MainWindow_Bim.cpp` |
| IO9 | moyenne | Export : schéma unique IFC4X3_ADD2, aucune sélection de catégories, fichier non relu après écriture | `onActionExportIfc` |
| IO10 | moyenne | Géoréférencement ignoré (`IfcMapConversion`, `IfcSite` RefLatitude…), unités d'angle ignorées | `IfcImporter` |
| IO11 | moyenne | Pas de conservation des données IFC non reconnues (classes, propriétés complexes, relations, géométrie d'origine) | `BimModel` (pas de stockage) |
| IO12 | basse | GlobalId des relations et des Psets recréés à chaque export (documenté) | `IfcExporter` |
| IO13 | basse | Aucun fichier IFC réel dans les tests ; aucun test IFC2X3 | `tests/data` |
| T1 | critique (hors I/O) | `ctest` ne trouvait **aucun test** (`enable_testing()` appelé dans une fonction) : corrigé le 17/10/2026 | `CMakeLists.txt` |

## 4. Données conservées par l'import actuel (cas favorable)

GlobalId (produits, projet, site, bâtiment, étages, objets analytiques), noms, descriptions, repères, types
prédéfinis, Psets à valeurs simples (`IfcPropertySingleValue` seulement), classifications, matériaux et profils
paramétrés, étages. Perdus : propriétés énumérées / bornées / listes / tables, quantités (`IfcElementQuantity`),
relations non structurelles, ouvertures, géométrie non paramétrique.
