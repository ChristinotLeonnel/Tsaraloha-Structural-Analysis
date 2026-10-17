# Validation de l'échange IFC — 17/10/2026

Branche `feature/io-ifc`. Correspondance des données : `docs/IFC_MAPPING.md` ; audit initial : `docs/AUDIT_IO.md`.
« Identique » désigne une **équivalence sémantique** mesurée par `IfcComparison` (mêmes GlobalId, classes, noms,
propriétés, états, étages, géométrie à une tolérance près, modèle de calcul). Aucune égalité binaire des fichiers n'est
revendiquée : un fichier réécrit par TSA diffère de l'original octet par octet (géométrie maillée, numéros d'entités,
GlobalId des relations).

## 1. Matrice des données BIM

Légende : **C** conservé · **V** converti · **A** approché · **P** perdu (à l'export). Preuves : tests de la suite `ifc`
(400–409) et `bim` (170–191), fichiers buildingSMART de `tests/data/ifc/buildingsmart`.

| Données | Import | Réexport IFC4X3_ADD2 | Preuve |
| :--- | :--- | :--- | :--- |
| GlobalId des produits, du projet, du site, du bâtiment, des étages | C | C | 400, 404, 179 |
| GlobalId des relations et des jeux de propriétés | P (non lus) | recréés | documenté (IO12) |
| Classe IFC d'origine | C (`PhysicalElement::ifcClass`) | C ; `…StandardCase` → classe de base + `TSA_Import.OriginalIfcClass` ; classe inconnue → `IfcBuildingElementProxy` + classe d'origine ; rétablie à la relecture par TSA | 400, 404 (21 fichiers) |
| Nom, description, type d'objet, repère, type prédéfini | C (nom absent conservé absent) | C | 404 |
| Unités | V (SI : pouce, mm → m ; aires, volumes, forces, contraintes) | V (écrit en SI) | 403 (pouces), 400 (mm) |
| Placements imbriqués | V (coordonnées du projet) | V (placement à l'origine de l'étage) | 403 |
| Géométrie | V (maillage du noyau OpenCASCADE : extrusions, B-rep, booléens, `IfcMappedItem`, ouvertures soustraites) | A (`IfcTriangulatedFaceSet`, couleur) ; représentation paramétrique d'origine **P** | 404 : écart d'emprise ≤ 9,5·10⁻⁷ m (tolérance 1 mm) |
| Propriétés et quantités | C (simples) ; V en texte (énumérées, listes, bornes, tables, références, complexes) | C (jeux de propriétés) | 404, 179 |
| Matériaux, profils | C (`TSA_Import.Materials`, `Profiles` ; matériau des éléments de calcul) | C (propriétés) ; matériau associé pour les éléments de calcul | 404 |
| Classifications | C | C (`IfcClassificationReference`) | 179, 404 |
| Hiérarchie spatiale, étages | C (projet, site, bâtiment ; un niveau par étage, sous son nom, **sans niveau du gabarit** dans un projet vierge) ; description de l'étage **P** | C (GlobalId, nom, cote) ; description non réécrite | 400 (« 00 groundfloor » seul), 404 (étages comparés), essai UI |
| Agrégations (tout → parties) | C (`TSA_Import.Parent`) | C (`IfcRelAggregates`, parties non contenues directement) | 401 |
| Ouvertures | V (soustraites, nombre `TSA_Import.Openings`) | P (`IfcOpeningElement` non réécrit ; géométrie percée conservée) | 402 |
| Portes / fenêtres (dimensions) | C (`OverallHeight`, `OverallWidth`) | C | 402 |
| Produits sans géométrie propre | C (état « partiel ») | C (sans représentation) | 401, 404 (toiture, cheminée, assemblages du pont) |
| Produits sans élément de calcul | C (« à préparer » / « non porteur », jamais supprimés) | C (maillage d'origine) | 400, 191 |
| Fondations et appuis | V (semelle / pieu relié à une connexion → fondation sur ce nœud) | C (`IfcRelAssignsToProduct` semelle → connexion) | 177, 179 |
| Voiles et surfaces de calcul | V (voile relié à sa surface → voile TSA reconstruit) | C | 179 |
| Modèle de calcul (`IfcStructuralAnalysisModel`) | C (nœuds, barres, surfaces, appuis, rotation γ) | C | 179, 404 (`structural_analysis_curve`) |
| Géométrie reconstruite ou déduite | **jamais en silence** : déduction seulement sur option, chaque élément signalé (remarque, compteur `derived`) | — | 178 (sans option : rien d'inventé ; avec : 3 éléments signalés) |
| Géoréférencement, grilles | **P** (non lus) | non écrits | limite documentée |
| Fichier tronqué ou non STEP | refus motivé, projet intact | — | 405, essai UI |

## 2. Export

| Exigence | Résultat | Preuve |
| :--- | :--- | :--- |
| Relecture obligatoire par un lecteur indépendant (IfcOpenShell) | faite avant d'écrire la cible ; échec si illisible | 177, 404, 409 |
| Écriture atomique | contenu vérifié écrit par `QSaveFile` ; cible existante remplacée seulement en cas de succès | 409 (cible protégée intacte) |
| Fichiers temporaires | copie de relecture `.tsa-ifc-*.ifc` supprimée, succès comme échec | 409 |
| Erreurs | dossier inexistant, cible protégée, fichier illisible : message, pas de faux succès | 409 |

## 3. Essais de l'interface réelle (UI Automation)

Scénario `tools/ui_tests/ifc_scenario.ps1` (vrais clics, vrais dialogues natifs, captures), exécuté de bout en bout
le 17/10/2026 sur `build-ninja-release` (TSA Release) : chaque résultat a été lu dans l'interface et capturé.

| Étape | Résultat observé |
| :--- | :--- |
| 1. Projet existant ouvert | « 8 nœuds · 9 élém. » |
| 2-4. Import IFC2X3, IFC4, IFC4X3_ADD2 | options (géométrie oui, modèle de calcul du fichier oui, déduction **non** par défaut) ; rapport (17 / 14 / 17 produits, onglets Objets, Par classe, Avertissements) ; **un nouvel onglet par import** ; niveau « 00 groundfloor » seul |
| 5. Annulation (grand fichier, 5 178 produits) | bouton Annuler pendant « Géométrie des produits » → « Import IFC annulé », aucun onglet créé |
| 6. Fichier tronqué | « fichier incomplet : fin « END-ISO-10303-21; » absente … Le projet ouvert n'a pas été modifié. », aucun onglet |
| 7. Bouton « Objets IFC importés » | coché → décoché : objets masqués, grille et aides visibles, « masqués (conservés dans le projet) » ; recoché : « affichés (16) » |
| 8. Survol et clic sur un objet | survol « IfcBuildingElementProxy « geo-reference » — non porteur » ; inspecteur « OBJET IFC : geo-reference » (classe, GlobalId, étage, état, géométrie, remarques, TSA_Import) |
| 9. Export | « relu par IfcOpenShell … 17 produit(s) », 1 étage ; fichier relu ensuite par `tsa-ifc` (17 produits, GlobalId et nom de l'étage d'origine) |
| 10. Onglet d'origine | toujours « 8 nœuds · 9 élém. » : projet intact après trois imports, une annulation et un échec |
| Fermeture | « Ne pas tenir compte » pour les trois projets importés non enregistrés ; réglages HKCU restaurés |

Constats : cadrage du projet importé fait au premier affichage de l'onglet (il englobe tous les objets, dont le repère
« geo-reference » placé à 30 m du bâtiment) ; titre de fenêtre « Sans titre » tant que le projet importé n'est pas
enregistré (BUG-077, cosmétique).

## 4. Points à faire confirmer par un spécialiste juridique

Voir `docs/THIRD_PARTY_LICENSES.md`. Restent à confirmer : suffisance de l'offre de source (archive jointe aux versions
publiées, § 6 d) GPL v3 par la LGPL), remplaçabilité des DLL C++ (même compilateur requis), extrait sous licence MIT
dans `clash_utils.cpp` (avis reproduit, auteur sans mention d'année), fichiers d'Eigen effectivement inclus (MPL 2.0
seule ou aussi LGPL), statut d'OpenSees (bloquant pour un usage commercial, inchangé).
