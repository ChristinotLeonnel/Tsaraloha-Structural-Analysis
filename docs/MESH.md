# Maillage — Gmsh dans TSA

Le maillage éléments finis de TSA est produit par **Gmsh 4.15.2** (SDK, API C++), encapsulé dans
`src/Mesh/Gmsh/GmshMeshEngine`. Ce document s'adresse aux utilisateurs (paramétrage, commandes) et aux
développeurs (architecture, correspondances, limites).

## 1. Architecture

```
Modèle TSA ──► MeshGeometry (barres, dalles + ouvertures, voiles, semelles, points caractéristiques, empreinte)
                     │
Paramètres (MeshSettingsSet : globaux, locaux, zones, profil)
                     ▼
MeshGenerator (façade) ──► GmshMeshEngine  ── seul composant qui appelle Gmsh
                     ▼
FeMesh (nœuds, éléments, groupes, qualité minSICN)  +  GenerationRecord (moteur, version, empreintes, interfaces, journal)
   │                 │                     │
MeshQuality     OpenSeesMeshAdapter    SurfaceLoads (charges nodales équivalentes)
   │                 ▼
   │        AnalysisContext.memberSubdivisions ──► CalculationSnapshot::subdivided ──► moteur ──► ResultsModel::mergeSubdivisions
   ▼
MeshProject (chunk MESH du .tsa)       Interface : MeshController (thread de travail), panneau, liste de qualité, MeshVisual (vue 3D, docs/MESH_VISUALIZATION.md)
```

| Couche | Fichiers | Dépend de |
|---|---|---|
| Données du maillage | `MeshTypes.h`, `MeshGeometry`, `MeshProject`, `MeshQuality`, `SurfaceLoads` | modèle TSA |
| Moteur | `MeshGenerator` (façade), `Gmsh/GmshMeshEngine` | SDK Gmsh (API C) |
| Conversion solveur | `Solvers/OpenSeesMeshAdapter` | capacités des moteurs d'analyse |
| Analyse | `CalculationSnapshot::subdivided`, `ResultsModel::mergeSubdivisions`, `AnalysisManager` | aucun maillage : seulement des positions de subdivision |
| Interface | `UI/Mesh/*`, `UI/MainWindow_Mesh.cpp`, `UI/MainWindow_MeshView.cpp`, `Viewer/MeshVisual` | tout ce qui précède |

Points de conception :

- **Aucune dépendance à Gmsh hors de l'adaptateur.** Les autres modules ne voient que `FeMesh` et des références TSA.
- **Aucune dépendance circulaire.** L'analyse ignore le maillage : elle reçoit des positions de subdivision par barre.
- **Paramètres du mailleur distincts des options du solveur.** Les tailles, algorithmes et ordres sont dans `MeshSettingsSet`. La transmission du maillage au calcul est l'option du solveur `CommonAnalysisSettings::useGeneratedMesh`.

## 2. Intégration de Gmsh

**SDK.** `thirdparty/gmsh-4.15.2-Windows64-sdk` contient l'en-tête `include/gmsh.h_cwrap`, la DLL `lib/gmsh-4.15.dll`, les licences et `bin/gmsh.exe`. Il n'est pas versionné : il se télécharge depuis gmsh.info.

**Compilateur.** Le SDK Windows est compilé avec MinGW (GCC 11). Son API C++ native n'est donc pas compatible avec l'ABI de MSVC. TSA utilise `gmsh.h_cwrap`, l'API C++ redéfinie sur l'API C, comme le documente le SDK.

**Bibliothèque d'importation.**
- À la configuration, `tools/dll_to_def.py` lit la table d'export de la DLL et n'en retient que l'API C (`gmsh*`, 383 symboles). `lib /def` produit ensuite `gmsh_msvc.lib`.
- La DLL est chargée en **différé** (`/DELAYLOAD`). Si elle est absente, TSA démarre quand même : le maillage est indiqué comme indisponible, sans plantage.
- Avant le premier appel, l'adaptateur vérifie la présence de la DLL avec `LoadLibrary`.

**Cycle de vie.**
- L'API Gmsh a un état global et n'est pas réentrante. Chaque opération (génération, import, export) ouvre une session, avec `initialize`, `General.Terminal = 0` et `logger::start`, puis la ferme avec `finalize`, sous un verrou global.
- Les erreurs de Gmsh (exceptions de `gmsh.h_cwrap`) sont remontées dans le rapport, avec les lignes « Warning » et « Error » du journal.

**Opérations longues.**
- La génération s'exécute dans un thread de travail (`MeshController`) : l'interface reste réactive (test 299).
- L'annulation est prise en compte **entre deux étapes** (géométrie, 1D, 2D, 3D, extraction). Gmsh ne s'interrompt pas au milieu d'une étape.

**Options CMake.**
- `TSA_WITH_GMSH` (activée par défaut) : maillage par Gmsh.
- `TSA_GMSH_SDK` : chemin du SDK.
- `TSA_PACKAGE_GMSH` (activée par défaut) : DLL incluse dans le paquet. **Licence GPL** : voir §10.

## 3. Construction de la géométrie Gmsh

La géométrie est construite avec le noyau « geo » de Gmsh, sans opération booléenne, pour garder une correspondance exacte :

| Objet TSA | Entités Gmsh |
|---|---|
| Nœud TSA | point (un seul par nœud, partagé par tous les objets) |
| Barre (B, C, T, K) | courbe(s) du nœud I au nœud J, découpées aux charges ponctuelles et aux bornes des portions de taille |
| Dalle (SL) | surface plane avec contours d'ouvertures (trous) |
| Voile (W) | rectangle médian (pied, offset, hauteur) |
| Semelle (F) | **4 volumes** autour de l'arête verticale passant par le nœud d'appui. Le nœud du poteau devient un sommet du maillage, sans empêcher le maillage hexaédrique |

La **conformité** est obtenue par construction :

- Une arête commune est **une seule courbe**. C'est le cas d'une barre sur un bord de dalle, de deux dalles voisines, du pied de voile sur la poutre. Les nœuds sont donc partagés.
- Sont **plongés** dans la surface (`mesh::embed`) :
  - un nœud TSA situé dans une dalle, par exemple un pied de poteau ;
  - une barre ou un pied de voile posés dans le plan d'une dalle, à l'intérieur.
- Les cas non conformes sont **signalés** dans le rapport (`MeshInterface`) et dans la liste de qualité, jamais masqués :
  - courbe recoupant un contour hors d'un sommet ;
  - barre partiellement confondue avec un bord ;
  - volumes jointifs ou superposés ;
  - liaison barre / volume par un seul nœud.

## 4. Paramètres

Le panneau **Paramètres du maillage** se trouve dans le ruban Analyse, groupe Discrétisation, ou dans le menu Calculs > Maillage (Gmsh).

| Paramètre | Traduction Gmsh |
|---|---|
| Taille globale × finesse × facteur, tailles min et max | champ `Constant` par objet (taille propre) ; `Mesh.MeshSizeMin` / `MeshSizeMax` |
| Ordre des éléments | `Mesh.ElementOrder`, `Mesh.SecondOrderIncomplete = 1` (Serendip : line3, tri6, quad8, tet10, hex20) |
| Algorithme 2D | `Mesh.Algorithm` (Automatique, MeshAdapt, Delaunay, Frontal-Delaunay, Frontal-Delaunay pour quadrangles, parallélogrammes, quadrangles quasi structurés) ; algorithme local par surface (`setAlgorithm`) |
| Maillage structuré | `setTransfiniteCurve` / `setTransfiniteSurface` sur les surfaces à 4 côtés **sans** ouverture, nœud plongé ni zone de raffinement. Nombres de nœuds égaux sur les côtés opposés, propagés entre surfaces voisines. Sinon : non structuré, et c'est consigné |
| Recombinaison | `setRecombine`, `Mesh.RecombinationAlgorithm` (Simple, Blossom, variantes « quadrangles seuls ») |
| Éléments de volume | tétraèdres (`Mesh.Algorithm3D` : Delaunay, Frontal, HXT ; `Mesh.Optimize`) ou hexaèdres **transfinis**, proposés pour les seuls volumes à 6 faces (semelles) |
| Lissage | `Mesh.Smoothing` |
| Divisions d'une barre | `setTransfiniteCurve(n + 1, "Progression" \| "Bump", coef)` : uniforme, progressive depuis le nœud de départ, symétrique (raffinée aux deux extrémités) |
| Longueur maximale, taille sur une portion | champs `Constant` sur les courbes concernées (portion = courbe découpée) |
| Raffinement aux extrémités | champ `Threshold` sur la distance aux nœuds, restreint à la barre (`Restrict`) |
| Divisions par direction (surface) | nombre de nœuds transfinis des côtés opposés |
| Zones de raffinement | `Threshold` sur une distance (`MathEval` pour un point, `Distance` pour un contour d'ouverture) : taille constante dans le rayon, croissance « transition » au-delà (`DistMax = rayon + (Hmax − h) / transition`), restreinte aux objets visés |
| Raffinement automatique | zones aux appuis, charges ponctuelles, changements de section, connexions et ouvertures |
| Nœuds caractéristiques | nœuds aux charges ponctuelles (courbe découpée), nœuds TSA plongés dans les dalles |
| Éléments de frontière | lignes des contours de surfaces, faces extérieures des volumes |
| Groupes physiques | voir §5 |
| Nombre maximal d'éléments | garde-fou : l'estimation et le résultat sont contrôlés |

Les options affichées sont celles de `meshEngineCapabilities()`. Ce que Gmsh ne sait pas faire n'est pas proposé : par exemple, aucun hexaèdre pour un volume qui n'a pas 6 faces.

Les commandes sont **Estimer**, **Aperçu**, **Générer**, **Régénérer**, **Vérifier la qualité**, **Supprimer**,
**Importer** et **Exporter**. S'y ajoutent **Maillage local de la sélection**, **Raffinement local autour de la
sélection** et **Ouvertures de la dalle** (menu Modélisation > Éléments surfaciques).

**Profils.**
- Profils de départ : rapide, standard, fin, très fin, personnalisé, poutres, dalles, voiles, ponts, volumes hexaédriques ou tétraédriques.
- Profils utilisateur : enregistrés dans `<configuration>/mesh-profiles.json`, avec import et export.
- Un profil est un ensemble de paramètres modifiables.

**Aperçu.**
- Le maillage prévisionnel est affiché en orange, sans remplacer celui du projet. Un second appel le retire.
- Une modification du modèle le retire aussi.
- L'aperçu ne remplace ni la génération ni son contrôle de qualité.

## 5. Groupes physiques et correspondances

Les **étiquettes Gmsh** (nœuds, éléments, entités) ne sont jamais utilisées comme identifiants : elles sont
renumérotées 1..N dans `FeMesh`. La correspondance est explicite :

| Groupe | Contenu |
|---|---|
| `B12`, `SL2`, `W5`, `F1`… | éléments de l'objet TSA (référence stable, comme en TSA3D) |
| `SL2:contour`, `F1:contour` | éléments de frontière (si demandés) |
| `appuis` | nœuds des appuis du modèle |
| `charges ponctuelles` | nœuds sous les charges ponctuelles |
| `matériau:<nom>` | éléments par matériau |
| `interface:A\|B` | nœuds des courbes partagées entre objets |

`FeNode::tsaNode` donne le nœud TSA confondu. `FeElement::source` donne l'objet d'origine. L'export `.msh`
écrit des groupes physiques Gmsh portant ces noms, et l'import retrouve les objets par ces noms.

## 6. Contrôle de qualité

`analyzeMesh` (liste interactive : sélectionner une ligne localise l'élément dans la vue 3D) vérifie :

- la qualité Gmsh **minSICN** : moins de 0 signale un élément inversé ;
- le rapport d'aspect, les angles et le jacobien normalisé (quadrangles, hexaèdres) ;
- les éléments dégénérés ;
- l'orientation incohérente entre éléments voisins ;
- les nœuds isolés et les nœuds confondus non fusionnés ;
- les connexions incorrectes : nœud inconnu ou répété, arête partagée par plus de deux éléments ;
- les incompatibilités entre régions : nœud d'une région au milieu d'une arête d'une autre ;
- les interfaces non conformes ;
- la croissance des tailles 1D ;
- l'estimation des DDL.

**Une génération sans erreur ne prouve ni la qualité ni la convergence.** Les tests 294 et 295 comparent les
résultats à des solutions analytiques.

## 7. Conversion vers OpenSees (`OpenSeesMeshAdapter`)

Avant tout calcul utilisant le maillage, l'adaptateur vérifie :

- le type et l'ordre des éléments ;
- la connectivité et l'ordre des nœuds : chaîne continue du nœud I au nœud J ;
- les DDL (`-ndm 3 -ndf 6`) ;
- les sections et matériaux ;
- les transformations et orientations ;
- les appuis, les charges et les groupes.

Les éléments non pris en charge sont listés avec une stratégie compatible. **Aucune conversion n'est silencieuse.**

| Élément du maillage | Conversion |
|---|---|
| line2 d'une poutre ou d'un poteau | **sous-éléments `elasticBeamColumn`**, avec l'orientation et la transformation de la barre et les relâchements aux seules extrémités |
| line2 d'un treillis ou d'un câble | un seul élément par barre : une chaîne articulée subdivisée serait un mécanisme |
| line3 | aucune : régénérer au premier ordre |
| tri3, quad4 (dalles, voiles) | **coques** `ShellDKGT` / `ShellMITC4` (formulation « coque » ; plaque et membrane seules refusées), section `ElasticMembranePlateSection`, nœuds partagés avec les barres — voir `docs/SURFACES.md` |
| tri6, quad8 | aucune coque OpenSees équivalente (`ShellMITC9` exige 9 nœuds) |
| tet4, tet10, hex8, hex20 | éléments de volume OpenSees à 3 DDL, incompatibles avec le modèle à 6 DDL de TSA : non transmis, les appuis aux nœuds restent appliqués |

Déroulé de la transmission des barres :

1. `AnalysisContext::memberSubdivisions` reçoit les positions relatives des nœuds intérieurs.
2. `CalculationSnapshot::subdivided` crée les nœuds intermédiaires et les sous-éléments.
3. Les charges sur barres sont réparties **exactement** : portions de charges réparties, charge ponctuelle sur le sous-élément porteur ou à un nœud du maillage. Le poids propre est calculé par sous-élément.
4. `ResultsModel::mergeSubdivisions` recompose les efforts et les stations sur la barre TSA. Les nœuds intermédiaires disparaissent des résultats ; leurs déplacements restent dans les stations.

Vérification sur des cas de référence :

| Test | Cas | Grandeurs vérifiées |
|---|---|---|
| 294 | poutre sur deux appuis, charge répartie | flèche 5qL⁴/384EI, réactions qL/2, moment qL²/8 |
| 295 | charge ponctuelle excentrée | réactions, moment Pab/L, flèche Pa²b²/3EIL |

Pour un maillage obsolète, l'utilisateur choisit : régénérer, calculer sans maillage, ou annuler.

## 8. Charges surfaciques (`SurfaceLoads`)

`distributeSurfaceLoad` répartit une pression sur les éléments 2D d'une surface :

- **directions** : X, Y, Z global, gravité, ou normale locale (une pression positive appuie sur la face) ;
- **surface** : réelle ou projetée ;
- **charges nodales cohérentes** : ∫ Nᵢ p dA par intégration de Gauss, pour tri3, tri6, quad4 et quad8.

La résultante et le moment par rapport à l'origine sont **contrôlés** contre l'aire nette (ouvertures déduites) et
son centre. Un écart, une surface non maillée ou un élément non pris en charge empêche la conversion, qui est
signalée.

Les charges surfaciques du modèle (`SurfaceLoad`, uniformes ou ponctuelles) et le poids propre des coques sont convertis par ce service au lancement du calcul, puis appliqués aux nœuds des coques (`docs/SURFACES.md`, tests 355 et 356). Une charge ponctuelle est répartie par les fonctions de forme de l'élément qui contient le point (`distributeSurfacePointLoad`).
aujourd'hui.

## 9. Enregistrement et reproductibilité

Le chunk `MESH` du `.tsa` (format 1.8) contient :

- les paramètres globaux et locaux, les zones et le profil ;
- le maillage généré (nœuds, éléments, qualité par élément, groupes) ;
- le rapport : mailleur et version de Gmsh, date, durée, empreinte des paramètres, empreinte de la géométrie, avertissements, erreurs, interfaces, paramètres effectifs, journal du moteur, synthèse de qualité.

Le chunk `OPNG` contient les ouvertures des dalles.

À la réouverture, les paramètres et le maillage sont restitués. Si la géométrie a changé (empreinte différente), le
maillage est signalé **obsolète** : dans la barre d'état, dans le panneau et au lancement du calcul.

**Ligne de commande.** `tsa-mesh` (livré dans le paquet) maille un projet `.tsa` ou un fichier d'échange `.tsa3d`
avec les paramètres enregistrés dans le projet, et écrit le résultat dans le format déduit de l'extension (`msh`, `vtk`,
`unv`, `inp`, `bdf`, `mesh`) :

```
tsa-mesh <projet.tsa | modèle.tsa3d> <sortie.msh> [taille globale m] [--3d] [--quads] [--order2]
```

Codes de sortie : 0 succès ; 1 échec (fichier illisible, Gmsh indisponible, maillage refusé) ; 2 usage.

## 10. Distribution et licence

**Dépendances.** La DLL `gmsh-4.15.dll` (89 Mo) n'importe que des DLL système de Windows. `tools/verify_package.py`
le contrôle. Elle embarque statiquement OpenCASCADE, FreeType et d'autres bibliothèques : ces symboles, internes à la
DLL, n'entrent pas en conflit avec les DLL OCCT de TSA.

**Paquet.** `package_TSA` copie la DLL avant l'analyse des dépendances (étape 2 bis), puis `licenses/Gmsh/LICENSE.txt` et `CREDITS.txt`. `MANIFEST.json` indique
`gmshIncluded`.

**Licence : GNU GPL v2 ou ultérieure, avec exceptions.** Distribuer TSA lié à Gmsh impose le respect de la GPL pour
l'ensemble distribué, ou une licence commerciale des auteurs de Gmsh. **C'est une décision juridique à prendre avant
toute diffusion.** Pour diffuser sans Gmsh, configurez `-DTSA_PACKAGE_GMSH=OFF` : TSA démarre, et le maillage est
indisponible.

## 11. Tests

La suite `--suite=mesh` contient les tests 289 à 299 :

| Test | Contenu |
|---|---|
| 289 | 1D : taille, divisions locales, progression, nœud de charge |
| 290 | dalle avec ouverture (aire nette exacte, aucun élément dans l'ouverture), dalle structurée |
| 291 | semelle en hexaèdres, tétraèdres, second ordre, volume exact, liaison au poteau |
| 292 | raffinement local |
| 293 | groupes physiques, interface poutre / dalle, import et export msh et vtk |
| 294 | conversion OpenSees comparée à une solution analytique |
| 295 | conservation des charges et propriétés |
| 296 | diagnostics de qualité et éléments non pris en charge |
| 297 | enregistrement, relecture, obsolescence |
| 298 | charges surfaciques |
| 299 | contrôleur et panneau : thread de travail, interface réactive, aperçu, obsolescence |

## 12. Limites

- Coques et volumes non transmis à OpenSees (voir §7). Le modèle TSA n'a pas de volumes génériques : seules les semelles sont maillées en 3D.
- Pas de charge surfacique appliquée au calcul (voir §8).
- Annulation entre les étapes de Gmsh seulement.
- Pas d'étude de convergence automatisée dans l'interface : les comparaisons de référence sont dans les tests.
- Les paramètres du maillage ne font pas partie d'annuler / rétablir (comme les réglages d'analyse).
- Maillage structuré limité aux surfaces à 4 côtés ; la loi « Bump » de Gmsh approche la répartition symétrique.
