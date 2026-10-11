# Audit scientifique des calculs de TSA — registre central

Version 0.1.0, audit du 12/10/2026. Ce document est le registre central des calculs effectivement implémentés. Pour chacun, il indique où le calcul se trouve, la méthode et les hypothèses, les tests, les défauts relevés et corrigés, et le niveau de validation atteint.

Il complète :
- `docs/BENCHMARKS.md` : banc de validation et écarts quantifiés ;
- `docs/SURFACES.md` : éléments surfaciques ;
- `docs/TORSION.md`, `docs/SECTIONS.md`, `docs/OPENSEES_RESULTS.md`.

## 1. Niveaux de validation

| Niveau | Signification |
|---|---|
| **V — validé** | comparé à une référence indépendante (solution analytique, catalogue, autre moteur) avec un écart quantifié et une tolérance justifiée (banc `tests/benchmarks` ou suites `audit`, `surfaces`, `torsion`) |
| **T — testé** | couvert par des tests automatisés de cohérence (équilibre, non-régression, persistance) sans référence indépendante complète |
| **I — implémenté, non validé** | code présent et utilisable, aucune validation quantitative |
| **A — absent** | non pris en charge ; l'interface ne le propose pas, ou le refuse avant calcul |

Un bouton, une classe ou une énumération ne suffit pas à classer une fonctionnalité au-delà de I.

## 2. Architecture de la chaîne de calcul

1. **Modèle TSA** : `Model`, `Section`, `Material`, `LoadManager`.
2. **Instantané de calcul** : `CalculationSnapshot`. Il comprend les appuis (blocages automatiques « fourche » signalés), les subdivisions issues du maillage et les coques.
3. **Moteur :**
   - **OpenSees 3.8.0** (processus externe, 3D, 6 DDL par nœud) : script Tcl généré par `OpenSeesAnalysisBuilder`, correspondance par `OpenSeesModelMap`, lecture par `OpenSeesResultsReader` ;
   - **Custom2D** : cœur scientifique TSALab `tsalab::planar`, méthode des déplacements sur ossature plane et solveur bande, adapté par `Custom2DAdapter`.
4. **Résultats et sorties** : `ResultsModel`, puis note de calcul (`NDCProjectNote`), vues et panneaux.

**Unités internes.** Longueurs en m. E en Pa dans le modèle, converti en kPa dans le script (calcul en kN). Charges en kN et kN·m. Masses en t (kN) ou en kg (N). Raideurs en kN/m (converties en mode N, voir §4).

**Repères.** Le repère global a Z vertical.
- **Barres :** axes locaux OpenSees, x de i vers j et z dans le plan (x, vecxz) (`ElementTransformation`).
- **Efforts :** N > 0 en traction ; moments selon la règle de la main droite.

## 3. Registre des calculs

### 3.1 Résistance des matériaux et éléments linéaires

| Calcul | Implémentation | Méthode, équations | Hypothèses | Tests | Niveau |
|---|---|---|---|---|---|
| Rigidité de barre (N, V, M, T) | OpenSees `elasticBeamColumn` (3D) ; TSALab planar (2D) ; `ElementTransformation` (basique → local → global) | matrice d'Euler-Bernoulli 12×12 ; K_g = Tᵀ·K_l·T | Euler-Bernoulli (cisaillement transverse **négligé**), section constante | test 369 (K_l = analytique à 1e-16, R orthogonale, 6 modes rigides, K = Σ K_e) ; RDM-01 à 10 | **V** |
| Treillis, câbles | `truss` / `corotTruss`, `InitStrain` (précontrainte ε₀ = T₀/EA) | effort normal seul ; rotations des nœuds de treillis bloquées | petits déplacements (truss), corotationnel (câbles) | RDM-07a/b (treillis 2D, 3D) ; suite `cables` | treillis **V**, câbles **T** |
| Relâchements d'extrémité | `-releasey/-releasez` (moments seulement) | rotule de flexion | N, V, T non relâchables (refusé par ModelValidator) | tests 152, 158 | **T** |
| Appuis rigides, élastiques | `fix` ; `zeroLength` sur nœud auxiliaire | raideurs kN/m, kN·m/rad | — | suite `supports`, test 362 (kN/N) | **V** |
| Charges nodales | `load` | repère global | une charge « locale » sur un nœud est signalée | RDM-01 à 07 | **V** |
| Charge uniforme sur barre | `eleLoad -beamUniform` (OpenSees) | — | — | RDM-03, 05b, 10a/b | **V** |
| Charge ponctuelle sur barre | `eleLoad -beamPoint` | — | — | RDM-04b | **V** |
| Charge trapézoïdale, uniforme partielle, couple réparti | TSA : `BeamElementLoad::fixedEndForces` (forces d'encastrement parfait) | Gauss 4 points (exact, intégrande de degré ≤ 4) ; couples : fonctions d'Hermite | barre prismatique | RDM-10c (triangulaire), RDM-10d (partielle) | **V** |
| Efforts et déformée le long des barres | `OpenSeesResultsReader::fillBeamStations` ; courbes du moteur plan | équilibre exact de la barre chargée (efforts) ; flèche reconstruite | — | RDM-03b, 04b : efforts exacts ; flèche **1e-4** (OpenSees), exacte (Custom2D) | **V** |
| Poids propre des barres | `beamElementLoads` (ρ·g·A, décomposé en repère local) | g = 9,81 m/s² | — | RDM-12 | **V** |
| Combinaisons | `loadPatterns` (cas × facteur) | superposition linéaire | coefficients saisis par l'utilisateur (aucun coefficient réglementaire codé en dur) | test 77 | **T** |
| Propriétés de section | `Section` (A, Iy, Iz, J, Iw, Wel, centre de gravité, Iyz) | formules fermées ; congés des profils laminés ; El Darwish & Johnston pour J | parois d'épaisseur constante (UPN, cornières : ailes inclinées et congés non modélisés) | test 365 (intégration indépendante, catalogues EN 10365 à 0,05 %), test 332 (J) | **V** |
| Contrainte normale de fibre | `DiagramGeometry::normalStressMPa`, banc `sigma` | σ = N/A ± My/Wy ± Mz/Wz (enveloppe) | théorie des poutres, sans cisaillement ni torsion ; axes principaux supposés parallèles aux axes de la section | RDM-01, 09 | **V** (sections symétriques) |
| Torsion de Saint-Venant | OpenSees (GJ) ; `Section::it()` | θ = T·L/(G·J) | gauchissement **non pris en compte** dans la rigidité (Iw calculé, affiché seulement) | RDM-08a/b, suite `torsion` (tests 321-334) | **V** (Saint-Venant) ; torsion gênée **A** |
| Excentricités, offsets rigides de barre | — | — | — | — | **A** |
| Déformation d'effort tranchant (Timoshenko) | — | — | — | — | **A** |
| Charges thermiques | — | — | refusées pour les surfaces ; inexistantes pour les barres | — | **A** |

### 3.2 Éléments finis et résolution

| Calcul | Implémentation | Méthode | Tests | Niveau |
|---|---|---|---|---|
| Assemblage, résolution K·U = F | OpenSees : `constraints Transformation`, `numberer RCM`, `system BandGeneral` (par défaut) | factorisation directe | test 369 : K symétrique, défini positif, résidu ‖KU − F‖/‖F‖ = 5e-10 ; mesures du §7 : 6 solveurs concordants à 2e-12 | **V** |
| Mécanismes, singularités | message « matrix singular » ; garde des déplacements non physiques (> 10 × l'envergure) ; λ ≤ 0 en modal | diagnostic utilisateur (appuis, relâchements, torsion libre) | tests Standards.7, 330, 155 | **T** |
| Équilibre global | `computeGlobalEquilibrium` | Σ F + Σ R, résidu de forces et de moments | tous les cas du banc (`reactionsum`) | **V** |
| Coques (dalles, voiles) | ShellMITC4, ShellDKGT ; forces nodales cohérentes ∫ Nᵢ p dA | maillage Gmsh ; résultantes aux points de Gauss | FEA-01 à 05, suite `surfaces` (354-364) | **V** |
| Qualité du maillage | `OpenSeesMeshAdapter` (angle minimal, rapport d'arêtes) | avertissement au-delà de 15° / 10 | test 356 | **T** |
| Analyse modale | `eigen` (LAPACK / Arpack), `modalProperties` | masses concentrées | FEA-05, FEA-06, test 359 | **V** |
| Statique non linéaire géométrique | OpenSees : `geomTransf PDelta` / `Corotational`, Newton, LoadControl | incrémental | RDM-11a (charge complète), RDM-11b (P-Δ, 0,03 %) ; corotationnel non validé | P-Δ **V** ; corotationnel **I** |
| Non-linéarité matérielle | — : tous les éléments sont élastiques | — | — | **A** |
| Temporel, spectre de réponse, pushover, flambement linéaire (valeurs propres de K_G), charges mobiles, thermique | — | — | — | **A** (non proposés) |

### 3.3 Vérifications réglementaires

Sources des clauses : EN 1990:2002+A1:2005, EN 1992-1-1:2004, EN 1993-1-1:2005. Les valeurs ci-dessous ont été vérifiées contre les valeurs recommandées publiées et les annexes nationales connues. **Les textes officiels des annexes nationales (payants) n'étaient pas accessibles** : les valeurs nationales restent à confirmer sur les textes d'origine.

| Vérification | Implémentation | Clauses | Paramètres | Limites de validité | Tests | Niveau |
|---|---|---|---|---|---|---|
| Coefficients partiels γ, ψ | `NationalAnnexConfig` | EN 1990 Tableau A1.1, A1.2(B) ; NF, DIN, BS EN /NA | γG = 1,35/1,00, γQ = 1,50, γC = 1,50, γS = 1,15, γM0 = 1,00, γM1 = 1,00 (DIN : 1,10), γM2 = 1,25 (BS : 1,10) ; ψ par catégorie A à H, neige, vent | combinaisons (6.10) seulement | Standards.2 | **T** (valeurs conformes aux valeurs recommandées) |
| Flexion simple EC2 | `ConcreteDesignEC2` | §3.1.7 (λ, η, εcu3), §6.1, §9.2.1.1 (As,min (9.1N), As,max) | fck, fyk, γC, γS, enrobage | section rectangulaire, axe fort (My), fck ≤ 90 MPa, sans redistribution ; aciers comprimés au-delà de μlu | test 366 (calculs manuels), Standards.10 | **V** |
| Flambement par flexion EC3 | `SteelDesignEC3` | §6.3.1, Tableaux 6.1, 6.2 ; classe Tableau 5.2 | courbe selon h/b, tf, nuance ; β = 1 dans la note | classes 1 à 3 ; classe 4 refusée ; ni torsion ni flexion-torsion ; fy non réduite pour t > 40 mm | test 366, Standards.11 | **V** (barre comprimée seule) |
| Résistances de section EC3 (N, M élastique, V) | `SteelDesignEC3` | §6.2.3, 6.2.4, 6.2.5 (Wel), 6.2.6 | — | sans interaction N-M-V, classe 3 (élastique) | Standards.11 | **T** |
| Effort tranchant EC2, ELS (fissuration, flèche), déversement, interactions N-M (EC3 §6.3.3), assemblages, dalles et voiles (ferraillage, poinçonnement), séisme (EN 1998) | — | — | — | marqués « non vérifié » dans la note | — | **A** |

Une analyse aux éléments finis correcte ne signifie pas qu'une structure satisfait une norme. La note de calcul sépare les résultats mécaniques des vérifications réglementaires, et liste les vérifications non effectuées.

## 4. Défauts relevés et corrections

| Prio. | Défaut | Conséquence | Correction | Test de non-régression |
|---|---|---|---|---|
| **P0** | Statique non linéaire : Δλ = « taille de pas » (0,05) pour 10 pas | **50 % des charges appliquées**, résultats présentés comme ceux de la charge complète | Δλ = 1/n ; facteur atteint lu (`getTime`) et signalé s'il diffère de 1 | RDM-11a |
| **P0** | Cornière : Iy, Iz sans termes de transport | rigidité divisée par environ 2 (L 100×100×10 : 84 cm⁴ au lieu de 180) | inerties centrales, Iyz exposé | test 365 |
| **P0** | UPN : Iz autour d'un axe non central | Iz divisé par environ 2 (UPN 200 : 82 cm⁴ au lieu de 171) | Huygens | test 365 |
| **P0** | Wel des sections dissymétriques calculé avec h/2, b/2 | contraintes **sous-estimées** (T, cornière, UPN) | fibre la plus éloignée du centre de gravité | test 365 |
| **P0** | EC2 : μ > μlu, As = M/(0,8·d·fyd) présenté comme valide | ferraillage faux, aciers comprimés absents | section doublement armée (As2, σs2) | test 366 |
| **P0** | EC3 : courbes de flambement a/b pour les HEA/HEB, b pour les pleins et les T | χ surestimé, **non sécuritaire** | Tableau 6.2 (h/b, tf, nuance) | test 366 |
| **P0** | EC3 : classe 4 vérifiée avec l'aire brute | Nb,Rd surestimé | classe (Tableau 5.2) ; classe 4 refusée | test 366 |
| **P0** | Raideurs des ressorts en mode N écrites sans conversion | appuis élastiques **1000 fois trop souples** en mode N | conversion kN → N | test 362 |
| **P1** | Note, panneau, ancien générateur : EC2 avec max(\|My\|, \|Mz\|) ou √(My² + Mz²) | moment d'axe faible vérifié avec la section d'axe fort | My seul ; flexion déviée signalée | test 366 (indirect), note |
| **P1** | Profils laminés sans congés | A −3,6 %, Iy −4,3 % (IPE 300) ; poids propre sous-estimé | congés inclus (catalogue à 0,05 %) | test 365, RDM-12 |
| **P1** | 112 `assert()` dans `test_standards.cpp` | vérifications **sans effet en Release** (NDEBUG) | vérifications actives | suite `standards` en Release |

Défauts de la phase « surfaces » corrigés auparavant : voir `docs/SURFACES.md`.

## 5. Note de calcul

La note de calcul (`NDCProjectNote`) comprend, entre autres :
- **Présentation et périmètre :** projet, version, moteur et version, date.
- **Modèle :** géométrie, matériaux, sections (A, I, W, J), appuis (ponctuels et linéiques), actions et combinaisons.
- **Méthode et vérification :** méthode avec équations, contrôle du modèle et équilibre global (résidus, contrôle indépendant de la charge verticale).
- **Résultats :** déplacements, réactions, efforts, éléments surfaciques (maillage, formulation, origine des valeurs, conventions), analyse modale.
- **Vérifications réglementaires :** clauses, paramètres, limites, vérifications non effectuées.
- **Validation et traçabilité** (nouveau chapitre) : traçabilité du calcul, rapport du banc de validation (cas, écarts, statut ; version différente ou rapport absent signalés), limites connues du logiciel.

Exports : HTML et PDF (existants). Pas d'export DOCX dans l'architecture actuelle : recommandation P2.

## 6. Formats de données

| Format | Rôle | Schéma, version | Validation |
|---|---|---|---|
| `.tsa` (binaire à chunks) | projet natif | `docs/TSA_FILE_FORMAT.md`, version 1.10 ; ajouts non cassants (clés JSON additives) | tests `io`, 358, 360, 361 (aller-retour) |
| TSA3D (JSON) | échange 3D et structurel (extensions, TSALab) | `docs/schemas/tsa3d-1.schema.json`, `docs/TSA3D.md` | `tsa3d-validate`, suite `tsa3d` |
| `tsa.surfaces/1` (JSON) | API des surfaces | `SurfaceApi::toJson` | test 364 |
| IFC 4 (STEP) | BIM : produits physiques et modèle analytique | `docs/IFC_MAPPING.md` | suite `bim`, test 364 |
| `tsa.benchmark/1` (JSON) | cas de validation | `docs/schemas/tsa-benchmark-1.schema.json` | `TSA_Benchmark --validate-only`, test 368 |
| `tsa.reference/1` (JSON) / CSV | références externes | `docs/schemas/tsa-reference-1.schema.json` | test 368 |
| `tsa.validation-report/1` (JSON), CSV, Markdown | rapports de validation horodatés | `reportJson` | test 368 |
| Script Tcl OpenSees | entrée du moteur, non public | généré à chaque calcul | — |

**Règles communes.**
- Unités explicites : kN, m, kPa.
- Identifiants stables : ceux du modèle, GlobalId IFC.
- Erreurs listées et fichier refusé.
- Aucun pointeur ni détail d'implémentation sérialisé.

## 7. Performances et GPU

### 7.1 Mesures

`TSA_Benchmark --performance`, Release, Windows 10, 12 cœurs logiques, Intel UHD Graphics. Portiques 3D (HEB 200 / IPE 300), statique linéaire.

| Nœuds | Barres | DDL | Préparation | Script | OpenSees | Lecture | Total |
|---|---|---|---|---|---|---|---|
| 27 | 42 | 108 | 1 ms | 1 ms | 34 ms | 1 ms | 39 ms |
| 486 | 1 125 | 2 430 | 8 ms | 18 ms | 91 ms | 27 ms | 150 ms |
| 1 521 | 3 848 | 8 112 | 36 ms | 58 ms | 299 ms | 92 ms | 504 ms |
| 3 179 | 8 330 | 17 340 | 74 ms | 141 ms | 807 ms | 196 ms | 1 254 ms |

Solveurs linéaires d'OpenSees, sur le même modèle de 17 340 DDL :

| Solveur | Temps | Écart des déplacements à BandGeneral |
|---|---|---|
| BandGeneral (par défaut) | 853 ms | — |
| BandSPD | 605 ms | 2e-14 |
| UmfPack | 852 ms | 7e-14 |
| SuperLU | 3 409 ms | 4e-14 |
| SparseGEN | 3 393 ms | 4e-14 |
| ProfileSPD | 4 069 ms | 2e-12 |

Plancher de 24 dalles (1 536 coques) : maillage et calcul en environ 1 à 1,5 s (test 363).

### 7.2 Décision GPU

**Calcul numérique : aucun backend GPU n'est implémenté.** Les mesures ne le justifient pas.
- **Résolution externe :** la résolution, 65 % du temps, est faite par le processus OpenSees, dont la distribution standard n'offre aucun solveur GPU. Un solveur GPU dans TSA imposerait de remplacer le moteur, ce qui n'est pas justifié ici.
- **Taille des systèmes :** à 17 000 DDL, le système bande est factorisé en moins d'une seconde sur CPU. Sur un GPU, les transferts CPU-GPU et la factorisation creuse effaceraient le gain à ces tailles.
- **Code propre à TSA :** préparation, script et lecture représentent environ 0,4 s pour 17 000 DDL. Il s'agit de texte et d'entrées-sorties, sans calcul vectoriel massif.
- **Matériel de ce poste :** un GPU intégré Intel, sans CUDA.

Le CPU reste le mode fiable par défaut. Aucun téléchargement de dépendance n'a lieu, et le démarrage ne dépend d'aucun GPU.

**Affichage : rendu OpenGL d'OCCT (accélération matérielle).**
- **Diagnostic :** le rendu réellement obtenu (fournisseur, carte, version, matériel ou logiciel) est journalisé au démarrage de la vue (test 370 : Intel UHD Graphics, OpenGL 4.6, matériel).
- **Repli :** `TSA_RENDER_COMPAT=1` active le contexte de compatibilité et le pipeline fixe pour les cartes anciennes ou les pilotes défaillants.

## 8. Recommandations

| Prio. | Problème | Risque | Bénéfice | Difficulté, dépendances | Validation |
|---|---|---|---|---|---|
| P1 | Cornières : axes principaux inclinés non pris en compte | flexion déviée mal représentée | rigidité correcte des cornières | moyenne : orientation de section α = ½·atan(2Iyz/(Iz − Iy)) | intégration et catalogue (Iu, Iv) |
| P1 | Cisaillement transverse des poutres (Timoshenko) | flèches sous-estimées des poutres courtes ou hautes | poutres-cloisons, voiles en barres | moyenne : `ElasticTimoshenkoBeam` OpenSees, aires de cisaillement | Timoshenko analytique |
| P1 | Excentricités et offsets rigides | modélisation des liaisons poutre-poteau excentrées | modèles plus fidèles | moyenne : `-jntOffset` | portique analytique |
| P1 | EC2 effort tranchant, ELS ; EC3 interaction N-M, déversement | vérifications incomplètes | note réglementaire complète | élevée ; textes normatifs | calculs manuels de référence |
| P1 | Flambement linéaire (charges critiques) | stabilité globale non évaluée | αcr (EN 1993-1-1 §5.2.1) | moyenne : OpenSees (géométrie P-Delta + eigen) | Euler (π²EI/(βL)²) |
| P1 | Comparaison avec Robot sur les cas du banc | non exécutée (aucun résultat disponible) | confiance des utilisateurs | faible : export CSV, outil prêt | rapport `--reference` |
| P2 | Flèche reconstruite entre nœuds (OpenSees) : écart 1e-4 | précision d'affichage | cohérence avec Custom2D | faible | RDM-03b, 04b |
| P2 | BandSPD pour la statique linéaire (−29 % à 17 000 DDL) | message de mécanisme à adapter (matrice non définie positive) | performance des grands modèles | faible ; tests de mécanismes | suites `standards`, `torsion` |
| P2 | Export DOCX de la note | diffusion | — | moyenne ; dépendance | — |
| P2 | Exécution parallèle de cas indépendants (plusieurs processus OpenSees) | — | combinaisons multiples plus rapides | moyenne | concordance série/parallèle |
| P2 | Corotationnel : à valider | — | grands déplacements | faible : cas de console en grande rotation (Elastica) | banc |

## 9. Reproduire

```
cmake --preset ninja-release && cmake --build --preset ninja-release
build-ninja-release\TSA_TestSuite.exe                     # 370 tests
build-ninja-release\TSA_TestSuite.exe --suite=audit       # tests 365, 366, 369, 370
build-ninja-release\TSA_TestSuite.exe --suite=benchmarks  # banc complet + outillage
build-ninja-release\TSA_Benchmark.exe --out rapports      # rapport de validation horodaté
build-ninja-release\TSA_Benchmark.exe --performance --out rapports
build-ninja-release\TSA_Benchmark.exe --reference mes_resultats.csv
```
