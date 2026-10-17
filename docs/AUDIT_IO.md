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

## 4. Données conservées par l'import initial (cas favorable)

GlobalId (produits, projet, site, bâtiment, étages, objets analytiques), noms, descriptions, repères, types
prédéfinis, Psets à valeurs simples (`IfcPropertySingleValue` seulement), classifications, matériaux et profils
paramétrés, étages. Perdus : propriétés énumérées / bornées / listes / tables, quantités (`IfcElementQuantity`),
relations non structurelles, ouvertures, géométrie non paramétrique.

## 5. État après travaux — 17/10/2026

Mesures `tsa-ifc roundtrip` (import, export IFC4X3_ADD2 relu par IfcOpenShell, réimport, comparaison), Release,
IfcOpenShell 0.9.0post1. « A/R » : aller-retour identique (identités, classes, noms, Psets, états, géométrie à 1 mm,
modèle de calcul).

| Fichier | Schéma | Produits | Géométrie | À préparer | Non porteurs | Export relu | A/R | Écart max | Durée |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: | :---: | ---: | ---: |
| Building-Structural | IFC2X3 / IFC4 / IFC4X3_ADD2 | 17 | 16 | 11 | 6 | 17 | oui | 1,2·10⁻⁷ m | 0,3 s |
| Building-Architecture | les trois | 14 | 12 | 6 | 8 | 14 | oui | 1,2·10⁻⁷ m | 0,3 s |
| Infra-Bridge | IFC2X3 | 68 | 48 | 37 | 31 | 68 | oui | 4,8·10⁻⁷ m | 1,3 s |
| wall-with-opening-and-window | IFC4 | 2 | 2 | 0 | 2 | 2 | oui | 2,4·10⁻¹⁶ m | 0,1 s |
| column-straight-rectangle-tessellation (pouces) | IFC4 | 1 | 1 | 1 | 0 | 1 | oui | 2,2·10⁻¹⁶ m | 0,1 s |
| revit2011_wall1, revit2014_multiple_bounded_halfspaces | IFC2X3 | 1 | 1 | — | — | 1 | oui | 1,2·10⁻⁷ m | 0,1 s |
| nested_mapped_item | IFC2X3 | 53 | 51 | 24 | 29 | 53 | oui | 9,5·10⁻⁷ m | 1,1 s |
| acad2010_walls | IFC2X3 | 8 | 8 | 8 | 0 | 8 | oui | 4,0·10⁻¹⁵ m | 0,1 s |
| geometrygym_great_court_roof (3,7 Mo) | IFC2X3 | 5 178 | 5 178 | 5 178 | 0 | 5 178 | oui | 1,2·10⁻⁷ m | 13 s (import seul : 4,9 s) |
| standard_case_element_beam, large_offset, faceted_brep, mapped_shape_transformation, tesselated_faceset, structural_analysis_curve | IFC4 / IFC2X3 | 1 à 18 | toutes | — | — | toutes | oui | ≤ 1,8·10⁻⁷ m | ≤ 1,5 s |
| invalid.ifc, fichier tronqué, texte non IFC, fichier absent | — | — | — | — | — | — | refusés, motif affiché | — | — |

| N° | Traitement |
| :--- | :--- |
| IO1 | **corrigé** : IfcOpenShell lit toutes les représentations (noyau OpenCASCADE) ; 21/21 fichiers légaux importés avec géométrie |
| IO2 | **corrigé** : échec motivé si rien n'est importé ; fichier tronqué refusé (enveloppe ISO 10303-21) |
| IO3 | **corrigé** : import dans un modèle temporaire, projet ouvert remplacé seulement après succès |
| IO4 | **corrigé** : tous les `IfcElement` conservés (à préparer / non porteurs / partiels), jamais supprimés par la synchronisation ; géométrie persistée dans le `.tsa` (chunk `BGEO`) |
| IO5 | **corrigé** : déduction depuis la géométrie seulement sur demande, chaque élément déduit signalé ; liens explicites du fichier convertis (voile ↔ surface, semelle ↔ appui) |
| IO6 | **corrigé** : aucune propriété inventée hors déduction demandée (matériau provisoire alors signalé) |
| IO7 | **contourné** : IfcOpenShell lit les fichiers (schémas exacts) ; le lecteur interne ne sert plus qu'au modèle de calcul et au repli |
| IO8 | **corrigé** : import asynchrone, progression, annulation, rapport détaillé avec CSV |
| IO9 | **en partie** : relecture indépendante après export (échec si illisible), écriture atomique, export asynchrone sur copie ; schéma unique IFC4X3_ADD2 et pas de sélection de catégories |
| IO10 | **ouvert** : géoréférencement ignoré (unités d'angle : avertissement du noyau) |
| IO11 | **corrigé** : classe d'origine, Psets et quantités, matériaux, profils, type, parent, ouvertures, géométrie (maillage) conservés et réexportés |
| IO12 | ouvert (documenté) |
| IO13 | **corrigé** : 9 fichiers buildingSMART réels (IFC2X3, IFC4, IFC4X3_ADD2) dans la suite `ifc` (tests 400–407) |

Défauts trouvés et corrigés pendant les essais : fondations exportées sans lien vers leur appui (l'ancien import les
déduisait en silence), produits sans géométrie propre omis à l'export, classe `…StandardCase` perdue, nom par défaut
écrit pour les produits importés sans nom, valeurs d'origine de `TSA_Import` écrasées au réimport, avis du noyau
présentés comme avertissements, en-tête de géométries `BGEO` faux si un maillage était nul.

Autres formats : pas de changement (inventaire § 1 inchangé ; `.tsa` passe en version 1.11 avec le chunk `BGEO`,
lu de façon tolérante par les versions antérieures qui l'ignorent). DXF reste absent.
