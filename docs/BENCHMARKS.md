# Banc de validation de TSA

Le banc de validation exécute des **cas de référence** par la chaîne de calcul réelle de TSA, puis compare chaque grandeur à une référence indépendante. La chaîne est la suivante : modèle TSA → `AnalysisManager` → moteur OpenSees (3D), Fem3D (3D) ou Custom2D (plan) → `ResultsModel`. Seules les analyses linéaires (statique linéaire, modale) sont proposées.

Le code se trouve dans :
- `src/Validation/BenchmarkSuite.{h,cpp}` ;
- `tools/benchmark/tsa_benchmark.cpp` (outil `TSA_Benchmark`) ;
- `tests/test_benchmarks.cpp` (suite `benchmarks`) ;
- les cas eux-mêmes : `tests/benchmarks/{rdm,fea}/*.json`.

## 1. Principes

**Références écrites en formules.** La valeur attendue est une expression évaluée à partir des paramètres du cas, par exemple `-5*q*L^4/(384*E*I)`. Aucun nombre n'est recopié à la main.

**Sources.** Chaque cas cite au moins une source : ouvrage de RDM, de théorie des plaques, catalogue, norme. Il énonce aussi ses hypothèses : modèle plan calculé en 3D, appuis, cisaillement négligé, etc.

**Tolérance mixte.** La comparaison utilise |x_TSA − x_réf| ≤ ε_abs + ε_rel·|x_réf|. La tolérance est propre à chaque grandeur et à chaque formulation ; il n'y a pas de tolérance universelle.
- **Solution exacte du modèle** (poutre d'Euler-Bernoulli, treillis) : ε_rel = 1e-6.
- **Approximation de discrétisation** (coques, masses concentrées) ou **hypothèse différente** (barres inextensibles) : ε_rel de 0,5 à 5 %. L'hypothèse est justifiée dans le cas.

**Valeurs interpolées.** Une valeur interpolée entre deux stations est marquée « valeur interpolée ». Elle n'est jamais présentée comme une valeur du moteur.

**Études de convergence.** Un cas peut demander plusieurs tailles de maille (`meshSizes`). Le rapport donne alors la suite des valeurs et la composition de chaque maillage (quadrangles, triangles). Les vérifications portent sur le maillage le plus fin.

**Absence de résultat.** Une grandeur absente des résultats est « NON DISPONIBLE », jamais remplacée. Un moteur absent du poste rend le cas « ignoré » : il n'est ni réussi ni en échec.

## 2. Format `tsa.benchmark/1`

Schéma : `docs/schemas/tsa-benchmark-1.schema.json`. Unités : kN, m, kPa. Le module E est en kPa, la masse volumique en kg/m³ ; en modal, les masses sont en t et les fréquences en Hz.

| Clé | Contenu |
|---|---|
| `id`, `title`, `category` (`rdm` / `fea`), `description` | identification |
| `sources`, `hypotheses` | références documentaires, hypothèses du modèle |
| `engines` | `opensees`, `custom2d` (moteur plan, portée « plan du modèle »), `fem3d` (cas ignoré si Fem3D n'est pas compilé) ; un autre nom est refusé à la lecture |
| `parameters` | valeurs numériques ou formules (paramètres dérivés : `"I": "b*h^3/12"`) |
| `model` | `materials`, `sections`, `nodes` (appuis `fixed`, `pinned` ou 6 DDL), `members` (`beam`, `column`, `truss`, relâchements `my`/`mz`), `slabs` (contour, épaisseur, appuis linéiques, ouvertures), `cases`, `nodalLoads`, `memberLoads` (`uniform`, `trapezoidal`, `point`, directions globales ou locales), `surfaceLoads` |
| `analysis` | `selfWeight`, `case`, `type` (`linear_static`, `modal` ; toute autre valeur, dont l'ancien `nonlinear_static`, est refusée à la lecture), `modes`, `subdivide` (sous-éléments par barre), `meshSize` / `meshSizes` / `quads` (surfaces), `engineSettings` (réglages propres au moteur, transmis tels quels) |
| `checks` | `quantity`, `label`, `expected` (nombre ou formule), `unit`, `tolerance` (`rel`, `abs`), `engines` (facultatif) |

### Grandeurs

Les identifiants de nœuds et de barres sont ceux du fichier JSON.

| Grandeur | Signification |
|---|---|
| `node:<id>:ux…rz` | déplacement ou rotation nodal (repère global) |
| `reaction:<id>:fx…mz` | réaction d'appui (repère global ; un moment de réaction est la composante globale autour de l'axe) |
| `reactionsum:fx/fy/fz` | somme des réactions |
| `member:<id>:<champ>:<sélecteur>` | champ `N`, `Vy`, `Vz`, `T`, `My`, `Mz`, `V` = √(Vy²+Vz²), `M` = √(My²+Mz²), `ux`, `uy`, `uz` (déplacements locaux), `sigma` = \|N\|/A + \|My\|/Wy + \|Mz\|/Wz ; sélecteur `start`, `end`, `absmax`, `max`, `min`, `at=<x/L>` |
| `surface:<composante>:absmax` | maximum (en valeur absolue, signé) sur les nœuds du maillage des surfaces |
| `modal:f<k>`, `modal:T<k>`, `modal:mass:x/y/z` | fréquence, période du mode k, masse totale |

**Conventions de signe.**
- Effort normal positif en traction.
- Rotations et moments selon la règle de la main droite, autour des axes globaux pour les nœuds et réactions, locaux pour les barres.
- Pour une poutre horizontale, z local est vertical vers le haut : une flèche vers le bas est négative.

## 3. Cas disponibles

Résultats du 31/10/2026, OpenSees 3.8.0, Fem3D 0.3.0 et Custom2D (TSALab MetDeDeplacement) : 39 cas, 2 097 vérifications, toutes réussies. Le 31/10/2026, l'analyse non linéaire a été retirée (ADR-025) : RDM-11a et RDM-11b, seuls cas non linéaires, ont été supprimés ; RDM-13, RDM-14 et FEA-08 à 10 ont été ajoutés (cas de l'audit du 31/10/2026). Les 141 vérifications des cas conservés donnent les mêmes valeurs, au bit près, qu'avant le retrait.

| Cas | Objet | Moteurs | Écart max. |
|---|---|---|---|
| RDM-01, 02 | barre en traction, en compression (δ = NL/EA, N, réaction, σ) | OpenSees, Custom2D | 4e-14 % |
| RDM-03 | poutre isostatique, charge uniforme (5qL⁴/384EI, qL²/8, qL/2, qL³/24EI) | OpenSees, Custom2D | 7e-14 % |
| RDM-03b | même poutre en une barre : moment et flèche aux stations | OpenSees (flèche 1e-3 %), Custom2D | 1e-3 % |
| RDM-04, 04b | charge ponctuelle centrale (PL/4, PL³/48EI) ; charge ponctuelle excentrée sur barre bi-encastrée (Pab²/L², flèche sous la charge) | OpenSees, Custom2D | 9e-3 % (flèche reconstruite, OpenSees) |
| RDM-05a, 05b | console, force en bout et charge uniforme (flèche, rotation, réactions) | OpenSees, Custom2D | 5e-14 % |
| RDM-06 | portique encastré, effort horizontal (méthode des rotations) | OpenSees, Custom2D | 0,41 % (déformation axiale, absente de la solution de référence) |
| RDM-07a, 07b | treillis plan (nœuds, travaux virtuels) et spatial (trépied) | OpenSees | 3e-12 % |
| RDM-08a, 08b | torsion circulaire (TL/GJ) et rectangulaire (série de Saint-Venant) | OpenSees | 0,085 % (approximation de Roark pour J) |
| RDM-09 | flexion composée (N/A + M/W) | OpenSees, Custom2D | 3e-14 % |
| RDM-10a à 10d | hyperstatiques : encastrée-appuyée, deux travées, charge triangulaire bi-encastrée (forces d'encastrement de TSA), charge uniforme partielle | OpenSees, Custom2D | 1e-13 % |
| RDM-12 | poids propre automatique (IPE 300, A du catalogue) | OpenSees, Custom2D | 4e-3 % |
| RDM-13a, 13b, 13c | rotules sous charge uniforme : encastrée-rotulée, poutre Gerber, barre à deux rotules (réactions, M = 0 à la rotule) | OpenSees, Fem3D, Custom2D | 9e-14 % |
| RDM-14a à 14g | rotule + charge ponctuelle, trapézoïdale, triangulaire ; poutre Gerber trapézoïdale ; deux rotules + charge ponctuelle ou trapézoïdale (non-régression de BUG-148) | OpenSees, Fem3D, Custom2D | 1,5e-7 % |
| FEA-08, FEA-09 | portiques 3D aléatoires, familles A et B (24 et 18 nœuds), référence : solveur indépendant de l'audit | OpenSees, Fem3D | 5e-8 % |
| FEA-10 | portique 3D aléatoire à rotules, famille C, charges ponctuelles et trapézoïdales (non-régression de BUG-148) | OpenSees, Fem3D | 2e-9 % |
| FEA-01 | plaque carrée appuyée, ShellMITC4 (Navier), convergence | OpenSees | +2,65 % (appui « mou » : couche limite de Mindlin, l'écart croît au raffinement ; voir FEA-07) |
| FEA-02 | même plaque, ShellDKGT, convergence | OpenSees | 0,08 % |
| FEA-03 | bande de coque en console (théorie des poutres) | OpenSees | 0,2 % |
| FEA-04 | dalle avec trémie, équilibre et aire nette | OpenSees | 1e-11 % |
| FEA-05 | mode propre d'une bande de coque | OpenSees | 0,49 % |
| FEA-06 | modes d'une poutre (flexion latérale et verticale, masses concentrées) | OpenSees | 4e-5 % |
| FEA-07 | même plaque, appui « dur », ShellMITC4 contre la référence de Mindlin (Wang et al. 2001), 4 maillages | OpenSees | 0,015 % (écart divisé par 4 à chaque raffinement : ordre 2) |

**Convergence des plaques** (flèche au centre, référence de Navier 2,444 mm) :

| Formulation | Maille de 1 m | 0,5 m | 0,25 m |
|---|---|---|---|
| ShellDKGT (Kirchhoff discret) | 2,392 mm | 2,429 mm | 2,442 mm (convergence monotone vers Navier) |
| ShellMITC4 (Mindlin) | 2,451 mm | 2,484 mm | 2,509 mm (tend vers la solution de Mindlin, légèrement plus souple) |

## 4. Exécution

```
TSA_Benchmark                                  # tous les cas, rapport dans ./validation-reports
TSA_Benchmark --cases tests/benchmarks/rdm --engine opensees
TSA_Benchmark --validate-only                  # contrôle des fichiers seulement (code 2 si un fichier est invalide)
TSA_Benchmark --reference resultats.csv        # comparaison à des résultats externes
TSA_Benchmark --performance                    # mesures de performance (docs/AUDIT_CALCULS.md §7)
TSA_TestSuite --suite=benchmarks               # même banc dans la suite de tests (rapport <build>/validation-reports)
ctest -R Benchmarks
```

**Rapports.** Ils sont écrits sous `report-<date>.{md,csv,json}`, avec une copie `latest.{json,md}`. L'historique n'est jamais écrasé.

**Paquet.** Le paquet livre :
- `validation/cases`, les cas relançables ;
- `validation/latest.json`, le rapport repris par le chapitre « Validation du logiciel et traçabilité » de la note de calcul ;
- `TSA_Benchmark.exe`.

**Codes de retour.**

| Code | Signification |
|---|---|
| 0 | aucun échec |
| 1 | au moins une vérification en échec ou non disponible |
| 2 | fichier invalide |

## 5. Comparaison avec un autre logiciel (Robot Structural Analysis…)

TSA ne lit pas les données internes d'un logiciel tiers. La comparaison se fait sur des résultats **exportés** au format CSV ou JSON (`tsa.reference/1`, schéma `docs/schemas/tsa-reference-1.schema.json`) :

```
case,quantity,value,unit,source,rel_tol,abs_tol
RDM-03,node:2:uz,<valeur>,m,Robot Structural Analysis <version> — modèle harmonisé,0.001,1e-9
```

Gabarit sans valeur : `tests/benchmarks/references/TEMPLATE.csv`. Sans tolérance précisée, la tolérance par défaut est de 2 %.

**Avant de comparer, harmoniser :**
- la géométrie, les matériaux et les sections (mêmes A, I, J : voir les congés des profils laminés) ;
- les axes locaux et l'orientation des sections ;
- les appuis (y compris les DDL hors plan), les excentricités et les relâchements ;
- les charges et leurs directions ;
- les unités et les cas ;
- le type d'élément : poutre d'Euler-Bernoulli ou de Timoshenko, coque de Mindlin ou de Kirchhoff ;
- les options du solveur et les conventions de signe.

Un écart n'est pas automatiquement une erreur de TSA. Le rapport donne la référence, la valeur TSA, les écarts absolu et relatif, la tolérance et la source, pour permettre d'expliquer l'écart (formulation, maillage, convention).

**Aucune comparaison avec Robot n'a été exécutée** dans ce travail : aucun résultat de ce logiciel n'était disponible. Les références indépendantes utilisées sont :
- les solutions analytiques ;
- les catalogues ;
- les deux moteurs de TSA entre eux : OpenSees et le cœur de calcul TSALab (méthode des déplacements, solveur bande).
