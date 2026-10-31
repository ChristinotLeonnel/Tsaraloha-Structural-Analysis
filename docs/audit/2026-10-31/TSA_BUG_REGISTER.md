# Registre des défauts — audit du 2026-10-31

Base auditée : commit `5403ed6`, binaires Release `build-windows-validation` (MSVC 19.51, Qt 6.12.0, OCCT 8.0.1, OpenSees 3.8.0).

Statuts employés :

- **CONFIRMÉ** : reproduit par une exécution réelle, preuve citée.
- **CONFIRMÉ (code)** : établi par lecture du code, non exécuté.
- **HYPOTHÈSE** : risque plausible, jamais présenté comme un défaut. Les hypothèses sont en fin de document.
- **CLOS PAR RETRAIT** : la fonctionnalité en cause a été retirée du logiciel ; le défaut reste ici comme défaut historique.

**Mise à jour du 2026-10-31 (après l'audit)** : l'analyse non linéaire a été retirée (ADR-025, branche `refactor/linear-only`, [LINEAR_ONLY_REFACTORING_REPORT.md](LINEAR_ONLY_REFACTORING_REPORT.md)). BUG-149 et BUG-150 sont clos par retrait, preuves ci-dessous. BUG-148 a été corrigé ensuite. Les autres défauts sont inchangés.

Les identifiants continuent la numérotation de `.claude/known-issues.md`. Tous les cas d'essai sont dans [preuves/](preuves/README.md).

## Synthèse

| Identifiant | Gravité | Sujet | Statut |
|---|---|---|---|
| BUG-148 | CRITICAL | OpenSees : rotule + charge non uniforme, efforts faux sans alerte | CONFIRMÉ, puis CORRIGÉ (2026-10-31) |
| BUG-149 | CRITICAL | Statique non linéaire : efforts le long des barres calculés avec un facteur de charge faux | CONFIRMÉ, puis CLOS PAR RETRAIT (2026-10-31) |
| BUG-150 | HIGH | P-Delta ou corotationnel accepté en statique linéaire, résultat incohérent | CONFIRMÉ, puis CLOS PAR RETRAIT (2026-10-31) |
| BUG-151 | HIGH | Câble comprimé accepté sans alerte | CONFIRMÉ |
| BUG-152 | HIGH | Note « Legacy » : flambement vérifié sur l'axe fort seul, verdict CONFORME | CONFIRMÉ |
| BUG-153 | HIGH | Cornières calculées sur leurs axes géométriques | CONFIRMÉ |
| BUG-154 | MEDIUM | `.tsa` à références pendantes accepté, violation d'accès au calcul | CONFIRMÉ |
| BUG-155 | MEDIUM | Exceptions non contenues à la préparation et dans les threads | CONFIRMÉ |
| BUG-156 | MEDIUM | Poids propre : règles différentes selon le mode, coefficient ignoré | CONFIRMÉ |
| BUG-157 | MEDIUM | Aucune sauvegarde automatique ni récupération après plantage | CONFIRMÉ (code) |
| BUG-158 | MEDIUM | Fem3D : coût de factorisation et boucles quadratiques | CONFIRMÉ |
| BUG-159 | LOW | `saveToFile` plante sans `QGuiApplication` | CONFIRMÉ |
| BUG-160 | LOW | TSALab : OpenSees laissé orphelin après le délai, code 259 pris pour un code de sortie | CONFIRMÉ |
| BUG-161 | LOW | Banc Fem3D/OpenSees : supprime ses références, synthèse trompeuse, fenêtres visibles | CONFIRMÉ |
| BUG-162 | LOW | Note : moteur et algorithme écrits en dur | CONFIRMÉ (code) |
| BUG-163 | LOW | Budget de l'historique sous-estimé | CONFIRMÉ (code) |
| BUG-164 | LOW | Marqueur de succès d'OpenSees jamais contrôlé | CONFIRMÉ |
| BUG-165 | LOW | Version d'OpenSees « non lue » au premier cas d'un lot | CONFIRMÉ |
| BUG-166 | LOW | Décompression sans plafond, CRC nul accepté | CONFIRMÉ (code) |
| BUG-167 | LOW | « Chiffrement » `.tsa` par XOR (non exposé dans l'interface) | CONFIRMÉ (code) |
| BUG-168 | LOW | Clé d'API en clair hors Windows | CONFIRMÉ (code) |

---

## BUG-148 — OpenSees : rotule + charge non uniforme, efforts faux sans alerte

- **Gravité** : CRITICAL. Résultat faux, sans message, sur le moteur par défaut, dans le sens défavorable à la sécurité.
- **Emplacement** : [OpenSeesAnalysisBuilder.cpp:824-842](../../../src/Analysis/OpenSeesAnalysisBuilder.cpp#L824-L842), [OpenSeesAnalysisBuilder.h:262](../../../src/Analysis/OpenSeesAnalysisBuilder.h#L262), [OpenSeesResultsReader.cpp:380-390](../../../src/Analysis/OpenSeesResultsReader.cpp#L380-L390), [OpenSeesAnalysisBuilder.cpp:843-848](../../../src/Analysis/OpenSeesAnalysisBuilder.cpp#L843-L848).
- **Description** : une barre portant une rotule (`release_i`, `release_j`) et une charge ponctuelle, trapézoïdale ou uniforme partielle donne de mauvais efforts avec OpenSees. Le moment à la rotule n'est pas nul et le moment dimensionnant est sous-estimé.
- **Cause racine** :
  1. Les charges `Kind::Linear` et `Kind::Couple` sont transmises aux nœuds par les forces d'encastrement d'une barre **bi-encastrée**, sans condensation de la rotule. Ces mêmes forces sont ensuite ajoutées aux efforts lus.
  2. Les charges ponctuelles passent par `eleLoad -beamPoint`. Le résultat montre qu'OpenSees 3.8.0 n'y applique pas la correction de rotule, alors qu'il l'applique à `-beamUniform` (le témoin uniforme est juste).
  3. Fem3D fait le calcul correct avec `condensedLoads`. Custom2D aussi.
- **Reproduction** : `TSA_Benchmark --cases preuves/cas/rotules`. Barre de 6 m, q = 12 kN/m ou P = 30 kN.

| Cas | Grandeur | OpenSees (TSA) | Référence | Écart | Fem3D, Custom2D |
|---|---|---:|---:|---:|---|
| `HINGE-02` ponctuelle, rotule en i | M encastrement | 13,33 | 26,67 | −50 % | justes |
| `HINGE-02` | M à la rotule | 26,67 | 0 | — | justes |
| `HINGE-03` trapèze constant | M encastrement | 36 | 54 | −33 % | justes |
| `PIN2-trap` deux rotules | M en travée | 36 | 54 | −33 % | justes |
| `PIN2-point` deux rotules | M en travée | 26,67 | 40 | −33 % | justes |
| `GERBER-02` poutre à rotule | M d'encastrement | 162 | 108 | +50 % | justes |
| `GERBER-02` | M en travée | 37,44 | 54 | −31 % | justes |
| `RND-C102` portique 3D, barre 6 | M extrémité j | 24,35 | 1,23 | ×19,8 | justes |

- **Portée mesurée** : 15 portiques 3D aléatoires comparés à un solveur indépendant. OpenSees échoue sur 5 portiques sur 5 de la famille avec rotules et charges non uniformes. Il réussit sur les familles sans rotule et avec rotules sous charge uniforme. La voie `Couple` (moment réparti) passe par le même code mais n'a pas été essayée avec une rotule.
- **Conséquence** : sous-dimensionnement possible d'une poutre articulée ou de son encastrement. Le contrôle d'équilibre **ne détecte rien**, car les forces nodales restent statiquement équivalentes. Les 9 projets d'exemple ne contiennent aucune rotule et ne sont pas touchés.
- **Solution proposée** :
  1. Immédiat : refuser avant calcul, dans `OpenSeesEngine::validate`, toute charge `Point`, `Linear` ou `Couple` sur une barre à rotule, avec un message orientant vers Fem3D.
  2. Correctif : condenser les forces équivalentes comme `condensedLoads` de Fem3D, et les charges ponctuelles aussi. Adapter `buildElementResults` pour reconstruire les efforts à partir des forces condensées.
- **Test de vérification** : ajouter `HINGE-*`, `GERBER-*`, `PIN2-*` à `tests/benchmarks/rdm` et les familles B et C à la suite `benchmarks`. Vérifier en plus M = 0 à chaque rotule.
- **Statut (2026-10-31)** : CORRIGÉ. Toutes les charges d'une barre à rotule, poids propre compris, passent par des forces d'encastrement condensées (`BeamElementLoad::fixedEndForces`), sans aucun `eleLoad` sur cette barre. Preuve : 57/57 suites, 547 tests ; tests 538-540 ; portiques aléatoires 12 678/12 678 (1 770 échecs avant) ; rotules 90/90 (21 avant) ; cases-more 0 échec (7 avant, 3 PDELTA-LIN refusés) ; banc officiel 2 097/2 097, 39 cas. Empreinte des 9 exemples identique à l'octet ; barres à rotule sous charge uniforme : écart d'au plus 3e-13. Détails : [preuves/bug-148](preuves/bug-148/). Les résultats déjà produits dans cette configuration restent à recalculer.

## BUG-149 — Statique non linéaire : facteur de charge faux dans les efforts le long des barres

- **Gravité** : CRITICAL. Toutes les barres chargées sont concernées pour ce type d'analyse, avec les réglages par défaut.
- **Emplacement** : [OpenSeesResultsReader.cpp:553](../../../src/Analysis/OpenSeesResultsReader.cpp#L553), à comparer à [OpenSeesAnalysisBuilder.cpp:905](../../../src/Analysis/OpenSeesAnalysisBuilder.cpp#L905).
- **Description** : le script applique Δλ = 1/`numSteps`, donc λ = 1 au dernier pas. Le lecteur calcule λ = pas × `stepSize`, soit 10 × 0,05 = 0,5 avec les valeurs par défaut.
- **Cause racine** : correction faite d'un seul côté. Le commentaire du générateur (ligne 903) décrit le défaut d'origine, le lecteur a gardé l'ancienne formule.
- **Reproduction** : `preuves/cas/non-lineaire`. Poutre isostatique de 6 m, q = 12 kN/m, 10 pas.

| Cas | Grandeur | TSA | Référence |
|---|---|---:|---:|
| `NLIN-control-lineaire` | M à mi-portée | 54 | 54 |
| `NLIN-uniforme` | M à mi-portée | 81 | 54 |
| `NLIN-uniforme` | M maximal | 107,73 | 54 |
| `NLIN-trapeze` | M à mi-portée | 45 | 54 |

- **Conséquence** : diagrammes et déformée intermédiaires faux, sous-estimés sous charge trapézoïdale. Les réactions restent justes, ce qui masque l'erreur.
- **Solution proposée** : utiliser dans le lecteur la même formule que le générateur, de préférence une seule fonction partagée, ou lire `TSA_OPS_LOAD_FACTOR` à chaque pas.
- **Test de vérification** : garder `cases-nl` dans le banc. `RDM-11a` ne contient que des charges nodales, il ne pouvait pas voir ce défaut.
- **Statut après retrait (2026-10-31)** : CLOS PAR RETRAIT. L'analyse statique non linéaire n'existe plus : ni type d'analyse, ni historique des pas, ni facteur de charge dans le lecteur (`OpenSeesResultsReader`), ni `TSA_OPS_LOAD_FACTOR`. Un ancien projet `nonlinear_static` est refusé avec un message, jamais calculé en linéaire (tests 532 et 535). `NLIN-uniforme` et `NLIN-trapeze` sont refusés à la lecture par le banc ; `NLIN-control-lineaire` donne les mêmes valeurs qu'avant. Le défaut n'est pas « corrigé » : la fonctionnalité qui le portait a disparu. La réintroduire exigerait de reprendre ce cas.

## BUG-150 — P-Delta ou corotationnel accepté en statique linéaire

- **Gravité** : HIGH.
- **Emplacement** : [OpenSeesEngine.cpp:150](../../../src/Analysis/Engines/OpenSees/OpenSeesEngine.cpp#L150), [OpenSeesAnalysisBuilder.cpp:925-940](../../../src/Analysis/OpenSeesAnalysisBuilder.cpp#L925-L940), [OpenSeesOptionsWidget.cpp:42-46](../../../src/UI/Analysis/OpenSeesOptionsWidget.cpp#L42-L46).
- **Description** : l'interface propose « P-Delta (2nd ordre) » quel que soit le type d'analyse. En statique linéaire, un seul pas `algorithm Linear` est exécuté, sans itération.
- **Cause racine** : aucune validation croisée entre type d'analyse et transformation géométrique.
- **Reproduction** : `PDELTA-LIN` (`RDM-11b` sans `nonlinear_static`).

| Grandeur | TSA | Second ordre attendu |
|---|---:|---:|
| Déplacement en tête | 1,505 mm (premier ordre) | 2,362 mm |
| Moment d'encastrement | 40,0 kN·m | 58,9 kN·m |
| Réaction horizontale | −9,778 kN | −10 kN |

- **Conséquence** : l'utilisateur croit avoir un calcul au second ordre. Moment sous-estimé de 32 %, et réaction horizontale incohérente avec la charge. Aucun avertissement.
- **Solution proposée** : refuser la combinaison, ou passer automatiquement en non linéaire avec message. Griser l'option en statique linéaire.
- **Test de vérification** : `PDELTA-LIN` doit être refusé, ou donner les valeurs de `RDM-11b`.
- **Statut après retrait (2026-10-31)** : CLOS PAR RETRAIT. L'option P-Delta et la transformation corotationnelle ont disparu de l'interface et du générateur (`geomTransf Linear` seul). Un ancien réglage `geomTransf` 1 ou 2, ou `trussFormulation` 1 ou 3, est refusé avant calcul avec un message (tests 533 et 537) : `PDELTA-LIN` passe de « 3 vérifications fausses sans alerte » à « calcul refusé ». Le second ordre n'est plus proposé du tout.

## BUG-151 — Câble comprimé accepté sans alerte

- **Gravité** : HIGH.
- **Emplacement** : [OpenSeesAnalysisBuilder.cpp:309-328](../../../src/Analysis/OpenSeesAnalysisBuilder.cpp#L309-L328).
- **Description** : un câble est un `corotTruss` sur matériau `Elastic`, qui résiste autant en compression qu'en traction.
- **Cause racine** : aucun matériau sans compression ni contrôle après calcul. Seule la note détaillée compte les câbles comprimés ([NDCProjectNote.cpp:1185](../../../src/NDC/NDCProjectNote.cpp#L1185)).
- **Reproduction** : harnais, essai B. Tirant vertical sous soulèvement de 30 kN : **N = −20,2 kN**, calcul réussi, 0 avertissement.
- **Conséquence** : un câble détendu continue de porter de la charge dans le modèle. Le schéma statique réel est différent, sans que rien ne l'indique dans les résultats.
- **Solution proposée** : avertissement bloquant après calcul pour tout câble à N ≤ 0. Ensuite, itération de désactivation des câbles détendus ou matériau sans compression en non linéaire.
- **Test de vérification** : essai B doit produire un avertissement dans `executionMetadata().warnings`.

## BUG-152 — Note « Legacy » : flambement sur l'axe fort seul

- **Gravité** : HIGH. Un verdict CONFORME peut être imprimé à tort.
- **Emplacement** : [NDCGenerator.cpp:874](../../../src/NDC/NDCGenerator.cpp#L874), disposition par défaut [ReportConfiguration.h:88](../../../src/NDC/ReportConfiguration.h#L88), aiguillage [NDCGenerator.cpp:111](../../../src/NDC/NDCGenerator.cpp#L111).
- **Description** : `calculateFromBar(..., L, 1.0, Ned, true)` vérifie le seul axe fort, avec Lcr = L, et imprime « CONFORME ».
- **Cause racine** : `strongAxis = true` écrit en dur. Les notes Simplifiée et Détaillée utilisent l'axe faible et affichent leurs limites ([NDCProjectNote.cpp:1293](../../../src/NDC/NDCProjectNote.cpp#L1293)), pas la note Legacy.
- **Reproduction** : harnais, essai D. S355, résistance imprimée et résistance sur l'axe faible :

| Profil | L | Nb,Rd imprimé | Nb,Rd axe faible | Surestimation |
|---|---:|---:|---:|---:|
| IPE 200 | 3 m | 942 kN | 266 kN | ×3,54 |
| IPE 200 | 5 m | 809 kN | 105 kN | ×7,68 |
| HEA 200 | 5 m | 1 400 kN | 732 kN | ×1,91 |
| HEB 300 | 5 m | 4 670 kN | 3 290 kN | ×1,42 |

- **Conséquence** : un poteau non conforme peut être déclaré conforme. Les formules EC3 elles-mêmes sont justes.
- **Solution proposée** : vérifier les deux axes et retenir le minimum, comme l'exige EN 1993-1-1 §6.3.1. Remplacer β = 1 par une donnée de l'utilisateur, en affichant l'hypothèse. Reprendre la mention des limites des autres notes.
- **Test de vérification** : poteau IPE 200, L = 5 m, N_Ed = 300 kN : doit sortir non conforme.

## BUG-153 — Cornières calculées sur leurs axes géométriques

- **Gravité** : HIGH.
- **Emplacement** : [Section.cpp:303](../../../src/Model/Section.cpp#L303) (`iyz()` jamais appelé hors de `Section`), [OpenSeesAnalysisBuilder.cpp:343-348](../../../src/Analysis/OpenSeesAnalysisBuilder.cpp#L343-L348).
- **Description** : pour une cornière, les axes y et z ne sont pas principaux. Le produit d'inertie est calculé mais n'est transmis à aucun moteur.
- **Cause racine** : `elasticBeamColumn` et Fem3D supposent y, z principaux. Aucune rotation vers les axes principaux, aucun avertissement.
- **Reproduction** : harnais, essai C. L100×100×10, console de 3 m, 1 kN en bout. Iy = Iz = 180 cm⁴, Iyz = −107 cm⁴, Iu = 287 cm⁴, Iv = 73,4 cm⁴.

| Grandeur | TSA | Flexion déviée |
|---|---:|---:|
| uz | −23,8 mm | −36,7 mm |
| uy | 0 | ±21,7 mm |

- **Conséquence** : flèche sous-estimée de 35 %, déplacement hors plan ignoré. Effet limité pour une barre travaillant uniquement en traction ou compression. Le centre de cisaillement des profils U, L et T n'est pas non plus pris en compte.
- **Solution proposée** : orienter l'élément sur ses axes principaux (angle ajouté à β), ou refuser les cornières fléchies, ou au minimum avertir. Documenter la limite sur le centre de cisaillement.
- **Test de vérification** : l'essai C doit donner les valeurs de flexion déviée à 1 % près.

## BUG-154 — `.tsa` à références pendantes accepté, violation d'accès au calcul

- **Gravité** : MEDIUM. Il faut un fichier abîmé ou fabriqué.
- **Emplacement** : [ModelValidator.cpp:576-578](../../../src/Standards/ModelValidator.cpp#L576-L578) (`a` et `b` déréférencés sans test), [TSAFile.cpp:953-1083](../../../src/IO/TSAFile.cpp#L953-L1083) (aucun contrôle des références après lecture).
- **Description** : un fichier dont une barre pointe vers un nœud absent s'ouvre sans erreur. Le lancement du calcul provoque ensuite une violation d'accès.
- **Cause racine** : pas de contrôle d'intégrité référentielle à la lecture, et `dirOf()` ne teste pas le pointeur, alors que la ligne 591 le fait.
- **Reproduction** : `preuves/fuzz/analyse_ACCESS_VIOLATION_00216.tsa`. 15 fichiers fuzzés sur 15 de cette famille tombent à la même étape.
- **Conséquence** : arrêt de l'application, perte du travail non enregistré (voir BUG-157).
- **Solution proposée** : contrôler les références en fin de `parsePayload` et refuser le fichier, ou l'ouvrir avec un rapport. Ajouter le test de pointeur.
- **Test de vérification** : rejouer `preuves/fuzz/` : aucun code ≥ 0xC0000000. Ajouter un test à la suite `io`.

## BUG-155 — Exceptions non contenues

- **Gravité** : MEDIUM.
- **Emplacement** : [Fem3DAdapter.cpp:52](../../../src/Analysis/Engines/Fem3D/Fem3DAdapter.cpp#L52) (hors `try`), [AnalysisManager.cpp:419](../../../src/Analysis/Engine/AnalysisManager.cpp#L419) (`validate` sans protection), [AnalysisController.cpp:110-112](../../../src/Analysis/AnalysisController.cpp#L110-L112) et [MeshController.cpp:85-92](../../../src/UI/Mesh/MeshController.cpp#L85-L92) (threads sans `try`).
- **Description** : une exception sortie de `prepare()` traverse le thread de l'interface. Une exception dans un `QThread::create` appelle `std::terminate`. Aucune surcharge de `QApplication::notify`.
- **Cause racine** : `Fem3DEngine::run` n'intercepte que `Fem3DError`, et `convert()` n'est protégé nulle part. Aucune frontière d'exception n'existe autour des appels de calcul.
- **Reproduction** : harnais, essai E, `Fem3DError : coordonnées non finies`. Tri du fuzzing : 8 fichiers sur 23 (4 `Fem3DError`, 4 `Standard_ConstructionError : gp_Vec::Normalize()`). Avec OpenSees, la même donnée est refusée proprement.
- **Conséquence** : fermeture brutale de l'application. Un `std::bad_alloc` dans le thread de calcul (grand modèle) suivrait le même chemin ; ce dernier cas n'a pas été reproduit.
- **Solution proposée** : frontière `try/catch (const std::exception&)` dans `AnalysisManager::prepare` et `run` et dans chaque corps de thread, avec conversion en message. Validation du modèle commune avant tout moteur.
- **Test de vérification** : essai E et fichiers `FAILFAST` : erreur affichée, processus vivant.

## BUG-156 — Poids propre : règles différentes selon le mode, coefficient ignoré

- **Gravité** : MEDIUM.
- **Emplacement** : [OpenSeesAnalysisBuilder.cpp:585-613](../../../src/Analysis/OpenSeesAnalysisBuilder.cpp#L585-L613), [OpenSeesResultsReader.cpp:1040-1049](../../../src/Analysis/OpenSeesResultsReader.cpp#L1040-L1049), [NDCProjectNote.cpp:809](../../../src/NDC/NDCProjectNote.cpp#L809).
- **Description** : pour un cas seul, la case du calcul décide et le drapeau du cas est ignoré. Pour une combinaison, le drapeau décide et la case est ignorée. `selfWeightFactor` n'est appliqué par aucun moteur, alors que la note l'utilise pour sa charge attendue.
- **Reproduction** : harnais, essai A, identique sur les trois moteurs. ΣRz sur une IPE 300 de 8 m (w·L = 3,315 kN) :

| Mode | Drapeau du cas | Case du calcul | Coefficient | ΣRz |
|---|---|---|---|---:|
| Cas seul | oui | oui | 1,5 | 3,315 (attendu 4,973) |
| Cas seul | **non** | oui | 1 | 3,315 |
| Cas seul | oui | **non** | 1 | 0 |
| Combinaison 1,35 | oui | **non** | 1 | 4,476 |
| Combinaison 1,35 | **non** | oui | 1 | 0 |

- **Conséquence** : un cas d'exploitation calculé seul inclut le poids propre, car la case est cochée par défaut. Le contrôle analytique de la note peut signaler un faux écart. Le coefficient n'est modifiable que par fichier ou import TSA3D.
- **Solution proposée** : décider d'une règle unique, l'afficher, et appliquer le coefficient ou le retirer du format.
- **Test de vérification** : table ci-dessus, une ligne par moteur.

## BUG-157 — Aucune sauvegarde automatique ni récupération

- **Gravité** : MEDIUM.
- **Emplacement** : aucun code correspondant dans `src/` ; [CrashHandler.cpp:253-254](../../../src/Diagnostics/CrashHandler.cpp#L253-L254) n'écrit qu'un rapport.
- **Conséquence** : tout plantage perd le travail depuis le dernier enregistrement. L'historique étant en mémoire, il est perdu aussi.
- **Solution proposée** : copie périodique par le chemin d'écriture atomique existant, sur un instantané, et proposition de récupération au démarrage.
- **Test de vérification** : arrêt forcé du processus après modification, puis récupération au lancement suivant.

## BUG-158 — Fem3D : coût de factorisation et boucles quadratiques

- **Gravité** : MEDIUM.
- **Emplacement** : [LinearSystemSolver.cpp:68](../../../../3DDesplacement/src/Solver/LinearSystemSolver.cpp#L68), [StructuralModel3D.cpp:51](../../../../3DDesplacement/src/Model/StructuralModel3D.cpp#L51), [BeamResponse.cpp:88-97](../../../../3DDesplacement/src/Results/BeamResponse.cpp#L88-L97).
- **Mesure** : 29 200 barres, 117 s contre 18 s pour OpenSees. Détail dans [TSA_PERFORMANCE_AUDIT.md](TSA_PERFORMANCE_AUDIT.md).
- **Cause racine** : `SimplicialLDLT` séquentiel, non supernodal, représente environ 90 % du temps mesuré par étape. `addElement` et `fromLoadCase` parcourent toute la liste à chaque appel.
- **Solution proposée** : solveur creux supernodal ou parallèle d'abord, index par élément ensuite. Les boucles quadratiques seules ne suffiraient pas.
- **Test de vérification** : temps total ≤ celui d'OpenSees sur le portique de 29 200 barres.

## Défauts de gravité LOW

| Identifiant | Emplacement | Constat | Preuve | Correction | Test |
|---|---|---|---|---|---|
| BUG-159 | [TSAPreviewGenerator.cpp:275](../../../src/IO/TSAPreviewGenerator.cpp#L275) | `saveToFile` s'arrête en `0xC0000409` avec un simple `QCoreApplication`. La garde `hasGuiApp` n'est pas complète. | Harnais, avant passage à `QGuiApplication` | Compléter la garde | Enregistrement depuis un outil en ligne de commande |
| BUG-160 | `TSALab/science/src/OpenSeesPlanarSolver.cpp:83-85` | Après 120 s, le processus n'est pas tué et le code 259 (`STILL_ACTIVE`) est lu comme code de sortie. | Processus 3612 orphelin de 14:10 à 14:16, sans sortie | `TerminateProcess`, puis message de délai | OpenSees factice qui ne rend pas la main |
| BUG-161 | `3DDesplacement/benchmarks/OpenSees/opensees_benchmarks.cpp:618-620` | `fs::remove` efface la référence suivie par Git avant le lancement. Si OpenSees ne produit rien, la synthèse est réécrite vide et conclut « cas en écart : 0 » avec 0 valeur comparée. `std::system` ouvre des fenêtres. | Incident du 2026-10-31, voir [résumé](TSA_AUDIT_EXECUTIVE_SUMMARY.md#incident-pendant-laudit) | Sortie dans un dossier temporaire, statut d'échec si rien n'est comparé, `CreateProcess` masqué | Lancement avec un OpenSees absent |
| BUG-162 | [NDCProjectNote.cpp:652](../../../src/NDC/NDCProjectNote.cpp#L652) | Le texte indique « moteur OpenSees, algorithme linéaire » quel que soit le calcul. | Lecture du code | Lire les métadonnées d'exécution | Note d'un calcul Fem3D |
| BUG-163 | [UndoManager.cpp:29-42](../../../src/UndoRedo/UndoManager.cpp#L29-L42) | L'estimation ignore charges, BIM, cotations et chaînes. Le budget de 512 Mio est donc dépassable. Mesure : 57,7 Mio par instantané à 29 200 barres. | Lecture du code, mesure | Compter les conteneurs omis | Comparaison estimation / mémoire réelle |
| BUG-164 | [OpenSeesSolver.cpp:311-334](../../../src/Analysis/OpenSeesSolver.cpp#L311-L334) | `TSA_OPS_SUCCESS` est écrit, jamais lu. OpenSees rend le code 0 sur une erreur Tcl. Le cas « tout vide » est intercepté. | Script `bad.tcl` | Exiger le marqueur | Script interrompu après `analyze` |
| BUG-165 | [BenchmarkSuite.cpp:774-783](../../../src/Validation/BenchmarkSuite.cpp#L774-L783) | Le premier cas d'un lot affiche « OpenSees (version non lue) ». | `bench-more`, `bench-nl` | Lire la version avant le calcul | Rapport de banc |
| BUG-166 | [TSAFile.cpp:200](../../../src/IO/TSAFile.cpp#L200), [TSAFile.cpp:779](../../../src/IO/TSAFile.cpp#L779) | `qUncompress` sans plafond, `uncompressedSize` ignoré à la lecture. Un CRC à 0 désactive le contrôle. | Lecture du code | Comparer à `uncompressedSize`, plafonner | Fichier à fort taux de compression |
| BUG-167 | [TSAFile.cpp:211-240](../../../src/IO/TSAFile.cpp#L211-L240) | Le « chiffrement » est un XOR avec le mot de passe. Non exposé dans l'interface. | Lecture du code | Retirer, ou chiffrement authentifié | — |
| BUG-168 | [SecretStore.cpp:29-40](../../../src/AI/Core/SecretStore.cpp#L29-L40) | Hors Windows, `protect()` rend la clé telle quelle. `isProtected()` le signale. | Lecture du code | Trousseau du système | Essai Linux |

## Hypothèses à vérifier (non confirmées)

| Identifiant | Sujet | Raison du doute | Vérification proposée |
|---|---|---|---|
| R-01 | Vue 3D et arbre sur un `.tsa` à références pendantes | Le fuzzing n'a pas couvert l'interface, pour ne pas ouvrir de fenêtres. Les appels `getNode` lus sont en général testés. | Suite `views` sur `preuves/fuzz/` |
| R-02 | Boucles `for (auto* obs : m_observers)` | Un observateur qui s'ajoute ou se retire pendant une notification invaliderait l'itération. Aucun cas trouvé. | Parcours sur copie, ou assertion en Debug |
| R-03 | `std::bad_alloc` dans le thread de calcul | Chemin établi par lecture (BUG-155), non reproduit. | Limiter la mémoire du processus d'essai |
| R-04 | OpenSees figé dans `tsalab_science_tests` | 0 octet de sortie en 6 min, cause non établie. L'entrée standard ouverte est écartée par essai (`stdin_probe.txt`). La copie de `thirdparty/OpenSees` a été remplacée depuis. | Relancer ce test sur le poste |
| R-05 | Greffons | `PluginManager::loadFile` charge sans signature. Le dossier est celui de l'application, ce qui limite le risque à une installation accessible en écriture. | Revue avant diffusion |
