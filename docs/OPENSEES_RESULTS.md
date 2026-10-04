# Extraction des résultats OpenSees — référence technique

Dernière mise à jour : 2026-10-04 · OpenSees **3.8.0** (exécutable externe `thirdparty/OpenSees/bin/OpenSees.exe`,
interpréteur Tcl, piloté par `QProcess`). Tous les comportements décrits ici ont été vérifiés sur ce binaire
(tests 105–111, suite `--suite=extraction`).

## 1. Chaîne de calcul

```text
Model (TSA)
  └─ CalculationSnapshot::capture          snapshot immuable ; éléments indexés par tag OpenSees unique
       └─ OpenSeesModelMap::build          correspondance TSA ↔ OpenSees (tags, axes, ressorts, recorders)
            ├─ OpenSeesAnalysisBuilder     script Tcl principal (inchangé : solveur, handler, numberer)
            │    └─ OpenSees → recorders   *.out (16 chiffres significatifs)
            │         └─ OpenSeesResultsReader::readResults      → ResultsModel (LIGHT)
            └─ [ADVANCED] buildMatrixScript (passage séparé, sans charge ni résolution)
                 └─ OpenSees → dof_map.out, kb_*.out, k_global.out
                      └─ OpenSeesResultsReader::readMatrixResults → ResultsModel::advanced()
```

Aucune autre partie de TSA n'appelle OpenSees. Les consommateurs (viewer, NDC, docks, export, contexte IA)
ne lisent que `ResultsModel`.

## 2. Identification des éléments

Les identifiants TSA ne sont uniques **que dans une famille** (`Model` a des compteurs séparés pour poutres,
poteaux, treillis, câbles). Un élément calculé est toujours désigné par `ElementKey {kind, id}`
(libellé `B12`, `C3`, `T7`, `K2`) et possède un **tag OpenSees unique** `1..N` attribué par
`CalculationSnapshot` (ordre Beam, Column, Truss, Cable puis id). Les charges sur barre sont résolues par
`CalculationSnapshot::findElementForLoad` (famille = `MemberLoad::targetType`).

Compatibilité : avant 2026-10-04, `MemberLoadDialog` enregistrait toujours `targetType = Beam`. Une charge
« Beam » sans poutre de cet id est rattachée à l'unique élément d'une autre famille portant cet id ; si
plusieurs familles le portent, la charge est ignorée et `ModelValidator::validateForAnalysis` l'annonce.

## 3. Nœuds, ressorts et degrés de liberté

- Modèle `BasicBuilder -ndm 3 -ndf 6` pour tous les nœuds : DDL `[UX UY UZ RX RY RZ]` (indices 0..5).
- Tag OpenSees d'un nœud = id du nœud TSA.
- Appui élastique : nœud auxiliaire entièrement fixé (tag > max id nœud TSA) + `zeroLength` (tag > N) avec un
  `uniaxialMaterial Elastic` par DDL. Les réactions du nœud auxiliaire sont **reportées sur le nœud TSA**.
- Mapping des équations (`AdvancedResults::dofMap`) : lu par `nodeDOFs` dans le passage matrices, avec le
  même `constraints` et `numberer RCM` que l'analyse principale. `-1` = DDL contraint (éliminé du système).
  Exemple : `Global DOF 0 → N3.UX`.

## 4. Efforts et conventions

| Grandeur | Source | Repère | Ordre | Signe |
| :--- | :--- | :--- | :--- | :--- |
| `ElementResults::startForces/endForces` | `localForce` (poutres/poteaux), `basicForce` (treillis/câbles) | local | N, Vy, Vz, Mx, My, Mz | **convention RDM TSA** : N > 0 en traction (`N_i = -localForce[0]`, `N_j = localForce[6]`), autres composantes telles qu'OpenSees |
| `ElementForceSet::local` | poutres : OpenSees `localForce` ; treillis/câbles : `T · globalForce` (exact) | local | [Fx Fy Fz Mx My Mz]_i,j | forces exercées **sur l'élément** par les nœuds |
| `ElementForceSet::global` | OpenSees `globalForce` (tous éléments) | global | [Fx Fy Fz Mx My Mz]_i,j | idem |
| `ElementForceSet::basic` | OpenSees `basicForce` | basique | poutres `[N, Mz_i, Mz_j, My_i, My_j, T]` ; treillis `[N]` | N > 0 en traction |

OpenSees 3.8.0 : `Truss` « localForce » renvoie 12 valeurs et `CorotTruss` ne reconnaît pas « localForce »
(seulement « localForces ») ; TSA utilise donc `basicForce` (1 valeur) pour l'effort normal des treillis/câbles.

Les **stations intermédiaires** (diagrammes) restent issues du post-traitement historique de TSA (interpolation
+ superposition approchée des charges) : voir known-issues BUG-016. Les valeurs d'extrémité et les forces brutes
ne passent pas par ce post-traitement.

## 5. Repère local (poutres/poteaux)

TSA transmet `geomTransf Linear $tag $vecxz` avec `vecxz` = axe z local TSA
(`CoordinateTransformationService::computeElementLocalFrame` : référence Z global, ou Y global pour un élément
vertical, puis rotation β autour de x). OpenSees calcule `x = (j−i)/L`, `y = (vecxz × x)/|…|`, `z = x × y`
(`LinearCrdTransf3d::getLocalAxes`) ; ce repère coïncide avec le repère TSA (test 109), donc les charges
locales `wy`, `wz` ont le même sens dans TSA et OpenSees.

`ElementTransformation` reproduit exactement LinearCrdTransf3d (sans excentricités) :

- `u_local = T · u_global`, T bloc-diagonale 12×12 de R (lignes = x, y, z dans le global) ;
- `ub0 = ul6 − ul0`, `ub1 = ul5 + (ul1 − ul7)/L`, `ub2 = ul11 + (ul1 − ul7)/L`,
  `ub3 = ul4 + (ul8 − ul2)/L`, `ub4 = ul10 + (ul8 − ul2)/L`, `ub5 = ul9 − ul3` ;
- `k_local = T_blᵀ · k_basic · T_bl`, `K_global = Tᵀ · k_local · T`.

## 6. Matrices (mode ADVANCED)

| Matrice | Source | Type | Repère | Exact |
| :--- | :--- | :--- | :--- | :--- |
| `k_basic` | OpenSees `basicStiffness` (ElasticBeam3d 6×6, Truss 1×1) | Initial tangent | basique | oui |
| `k_local` | reconstruite : `T_blᵀ k_basic T_bl` | Initial tangent | local | oui en `geomTransf Linear` ; **non** en PDelta/Corotational (rigidité géométrique absente) |
| `K_global` élémentaire | reconstruite : `Tᵀ k_local T` | Initial tangent | global | idem |
| `K_global` système | OpenSees `printA -ret`, `system FullGeneral`, passage séparé | Initial tangent (formTangent à l'état de référence) | global réduit (équations) | oui, 11 chiffres significatifs |

- **Contraintes** : `K_global` est la matrice du système **après** `constraints` (Transformation/Plain : DDL
  fixés éliminés). Elle inclut ressorts et câbles. Avec Penalty/Lagrange, la matrice contient pénalités ou
  multiplicateurs ; avec Lagrange les équations supplémentaires ne correspondent à aucun nœud et la matrice est
  refusée (taille incohérente avec `nodeDOFs`).
- **Pourquoi un passage séparé** : seul `system FullGeneral` expose sa matrice dans OpenSees 3.8.0
  (`BandGeneral`, `SparseGEN`, `UmfPack` → `printA` vide, vérifié). Le solveur de l'analyse n'est pas modifié.
  `initialize` numérote les DDL sans résoudre (un câble précontraint n'est donc pas déplacé), `record` écrit les
  rigidités basiques.
- **Plafond** : `AnalysisParameters::maxGlobalStiffnessDofs` (1 500 par défaut). Au-delà, `K_global` n'est pas
  extraite (raison dans `kGlobalUnavailableReason`) ; mapping DDL et matrices élémentaires restent disponibles.
- **Stockage** : COO creux trié (`SparseMatrix`), jamais de copie dense dans TSA ; le dock l'affiche via un modèle
  de table paresseux.
- `CorotTruss` (câbles, formulation corotationnelle) : pas de `basicStiffness` dans OpenSees 3.8.0 → matrice
  élémentaire indisponible (contribution présente dans `K_global`).
- Analyses non linéaires : les matrices sont **initiales**, pas la tangente du dernier pas.

## 7. Unités

Celles du script (`useKiloNewtons`) : kN, m, kN·m, kN/m, kN·m/rad (ou N…). `ResultsModel::units()` les déclare ;
aucune conversion n'est appliquée. `E` TSA (Pa) est multiplié par 1e-3 en kN ; charges TSA (kN) multipliées par
1000 en N. Le contrôle d'équilibre applique les mêmes facteurs (corrigé le 2026-10-04 : les charges n'étaient pas
converties en N).

## 8. Sécurité numérique

- Valeurs non finies (NaN/Inf/IND) dans un fichier de résultats → résultats invalidés, jamais stockés.
- Ligne de recorder dont le nombre de valeurs ne correspond pas au mapping → résultats rejetés (plus de lecture
  décalée).
- `OpenSeesModelMap::validate` avant calcul : tags dupliqués, nœuds absents, repère indéfini, collisions de tags
  des ressorts.
- `nodeDOFs` doit couvrir tous les nœuds et numéroter 0..n−1 sans trou ni doublon, sinon matrices ignorées.
- Script Tcl à 17 chiffres significatifs (l'ancien format `std::fixed` à 12 décimales tronquait Iz ≈ 3·10⁻⁶ m⁴
  d'environ 3·10⁻⁷ en relatif).

## 9. Précision mesurée (tests)

| Contrôle | Écart relatif |
| :--- | :--- |
| K_global printA vs Σ K_e reconstruites + ressorts (portique 3D) | 4,7·10⁻¹² |
| K·U − F (charges nodales) | 7·10⁻¹⁰ (11 chiffres de printA) |
| K_e·u_e − globalForce ; T·global − localForce ; k_b·u_b − basicForce | ~10⁻¹⁴ |
| Σ F + Σ R (ressort, charges réparties, poids propre, kN et N) | ~10⁻¹⁵ |
| Console : flèche PL³/3EI, rotation, réactions, 12EI/L³, 4EI/L, EA/L | < 10⁻⁹ |

## 10. Coût (Debug, ce poste)

| Modèle | Équations | LIGHT | ADVANCED (dont passage matrices) | Mémoire avancée TSA |
| :--- | ---: | ---: | ---: | ---: |
| 70 nœuds / 93 éléments | 210 | 73 ms | 198 ms (129 ms) | 284 Kio |
| 234 nœuds / 329 éléments | 702 | 111 ms | 1 154 ms (990 ms) | 1 005 Kio |
| 352 nœuds / 501 éléments | 1 056 | 403 ms | 2 422 ms (2 185 ms) | 1 530 Kio |

Le passage matrices est dominé par `printA -ret` (n² valeurs formatées par Tcl) ; mémoire transitoire OpenSees
≈ 8n² octets + chaîne Tcl ≈ 17n² octets (≈ 55 Mio à 1 500 DDL). En LIGHT, rien n'est ajouté.

## 11. Export et contexte IA

- `ResultsExport` (seul système d'export de résultats) : CSV / JSON / TXT pour déplacements, réactions, efforts,
  mapping DDL, K_global (COO avec libellés `N12.UZ`), rigidités élémentaires ; métadonnées incluses.
- `ResultsContext::nodeContext / elementContext` : JSON structuré (déplacement, réaction, éléments connectés,
  efforts au nœud, forces brutes, bloc 6×6 nodal des K_e, diagonale de K_global, cas de charge, unités,
  validité des résultats).
- UI : dock « Données d'analyse (OpenSees) » (menu Résultats) ; choix LIGHT/ADVANCED et plafond dans
  « Configuration du calcul ».
