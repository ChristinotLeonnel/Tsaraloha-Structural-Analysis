# Known Issues

Last Updated: 2026-11-02 (BUG-153 corrigé et retiré, test 586 — reste documenté : centre de cisaillement, docs/AUDIT_CALCULS.md § 8 ; BUG-151 corrigé et retiré, test 585 ; BUG-152 corrigé et retiré, test 584 ; BUG-149 et BUG-150 retirés : fonctionnalité retirée, analyse non linéaire, ADR-025 ; BUG-177 et BUG-178 corrigés et retirés, tests 583 et 373, voir changelog). Avant : 2026-11-01 (BUG-175, BUG-176 ajoutés ; BUG-174 ajouté ; BUG-074 corrigé et retiré, test 573 ; BUG-173 ajouté). Avant : 2026-11-01 (bugs corrigés retirés). Avant : 2026-10-31 (BUG-148 corrigé ; analyse non linéaire retirée, ADR-025 : BUG-149 et BUG-150 clos par retrait, conservés ; BUG-151 à 168 inchangés). Avant : audit technique du 2026-10-31 (BUG-148 à 168). Avant : 2026-10-26 soir (préparation de l'étape 2 : BUG-006 recompté, BUG-125 à BUG-130 caractérisés, non corrigés). Avant : 2026-10-26 (D1-D6 : BUG-117 P1 corrigés, BUG-111 étape 0, BUG-118 étape E0, BUG-113 tests ajoutés, BUG-110 présenté). Précédent : 2026-10-25 (TSA Core phase 0 : BUG-105 à BUG-109 corrigés, BUG-110 à BUG-115 ouverts ; portage Linux : BUG-091 à BUG-094, BUG-096 à BUG-098 corrigés ; BUG-095, BUG-099 à 101, BUG-103 corrigés le 2026-10-25 ; BUG-102, BUG-104 ouverts ; BUG-076 complété). Un bug entièrement corrigé est retiré de ce fichier (2026-11-01 : 23 entrées FIXED et la section « Corrigés (historique) », 16 entrées, retirées) ; son historique reste dans Git et dans changelog.md. Un bug corrigé en partie reste, avec ce qui demeure ouvert. Un bug d'une fonctionnalité retirée (ADR-022 dynamique, ADR-025 non linéaire) est retiré aussi : il revient avec la fonctionnalité.

## Ouverts

Audit technique du 2026-10-31 : détail, reproduction et correction de BUG-148 à BUG-168 dans docs/audit/2026-10-31/TSA_BUG_REGISTER.md.

## BUG-175
Status: OPEN (MEDIUM, à vérifier, relu dans le code le 2026-11-01, non reproduit) — contenu comparé par Annuler incomplet :
`TSAFileWriter::snapshotContentBytes` n'écrit pas les charges surfaciques (dans `loadSnapshot` mais écrites par le chunk SURF), ni
les ouvertures, formulations et appuis de bord des surfaces, ni les propriétés des sections (chunks OPNG, SURF, SECP construits à
part). `UndoManager::sameContent` pourrait donc juger identiques deux états qui ne diffèrent que par ces données, et une
transaction ne changeant qu'elles ne créerait aucune entrée Annuler — sauf si `ModelDiff` (accélérateur) les détecte : non vérifié.
L'empreinte des résultats enregistrés (TSAProjectIO::modelFingerprint) les inclut déjà (étape 4). Piste : écrire ces données dans
snapshotContentBytes depuis l'instantané (fonctions Detail::openingsChunkJson / surfacesChunkJson à rendre utilisables sur un
instantané). Test à écrire : transaction qui ne change qu'une charge surfacique.
Related files: src/IO/TSAFile.cpp (snapshotContentBytes), src/UndoRedo/UndoManager.cpp (sameContent)

## BUG-176
Status: OPEN (LOW, limite connue de l'étape 4, 2026-11-01) — après un calcul, le document est marqué « à enregistrer » (résultats du
bloc RSLT) ; mais Annuler jusqu'à l'état enregistré le rend « non modifié » (UndoManager::matchesClean ne compare que le modèle) alors
que les nouveaux résultats ne sont pas dans le fichier. Fermer sans enregistrer perd alors ces résultats (recalculables), jamais une
donnée du modèle. Piste : mémoriser au point d'enregistrement une marque des résultats enregistrés et la comparer aussi.
Related files: src/UndoRedo/UndoManager.cpp, src/UI/MainWindow_Tools.cpp (onAnalysisResultsChanged)

## BUG-174
Status: OPEN (LOW, intermittent, vu une fois le 2026-11-01) — suite `display`, test 371 (« aucun objet de résultat orphelin ») :
36 objets affichés pour 35 attendus (`shown == withModel + d1.total()`, tests/test_display*.cpp:251), suite terminée en 1,6 s au lieu
de 7 s, pendant une suite complète (Release, branche fix/save-dirty-state, étape 2). Relancée seule : 5/5 réussie ; suite complète
précédente : réussie. Aucun lien établi avec l'étape 2 (fonctions ajoutées dans DisplayProfile.cpp, appelées seulement par MainWindow,
non lié aux tests). À surveiller : capturer la sortie complète si cela se reproduit.

## BUG-173
Status: OPEN (LOW, environnement, constaté le 2026-11-01) — suite `benchmarks`, test 367 en échec sur un poste sans le moteur Fem3D
(dépôt voisin `../3DDesplacement` absent) : 1 140 vérifications réussies, 0 en échec, mais 957 « non disponibles » (13 cas fem3d ignorés,
« moteur indisponible ») et le test exige `checksUnavailable() == 0`. Sans lien avec le code testé. Pistes : déclarer `fem3d` dans
`--available` du lanceur (run_test_suites.py) et ignorer ses cas quand il manque, ou cloner 3DDesplacement.
Related files: tests/test_benchmarks.cpp:71, tools/ci/run_test_suites.py

## BUG-171
Status: OPEN (MEDIUM, relu dans le code le 2026-10-31, non reproduit par un test) — symétrie sans copie (Model::mirrorElements, keepOriginal = false) :
le contour de la dalle est inversé, mais ses ouvertures, appuis de bord et charges surfaciques en (u, v) ne sont pas convertis ; ils se retrouvent
ailleurs sur la dalle. La copie miroir, elle, les convertit (CopyTransform.h). Correction : appliquer mirroredContour, surfacePointMap et surfaceLoadImage
à ce chemin. Voir docs/audit/2026-10-31/COPY_PASTE_AUDIT_AND_FIX_REPORT.md § 6.

## BUG-170
Status: OPEN (LOW, vu une fois le 2026-10-31, non reproduit) — suite `surfaces` : 12 tests réussis, ligne RESULTS écrite, puis processus
bloqué jusqu'au délai de 600 s (DÉLAI DÉPASSÉ, 622 s au lieu de 10 s), sur le bureau masqué, branche feature/coordinate-axis-gizmos. Blocage à
la sortie du processus, après les tests. 10 autres exécutions dont 6 répétées : 10 s, code 0. Cause inconnue ; lien avec les repères ni
établi ni exclu (tests/test_surfaces.cpp ne crée aucune OccView). À surveiller : capturer la pile si cela se reproduit.

## BUG-169
Status: OPEN (LOW, reproduit le 2026-10-31) — test 457 (suite i18n) : seuil de 50 ms non tenu en Debug (pire recherche dans un
index synthétique de 6 000 entrées : 347 à 410 ms, sans autre charge ; 18 ms en Release). L'arrêt au premier échec (BUG-104)
empêche ensuite les tests 458 à 463 de tourner en Debug. Correction : seuil dépendant du type de compilation, ou mesure en
Release seulement.

## BUG-154
Status: OPEN (MEDIUM, reproduit) — .tsa à références pendantes accepté, violation d'accès au calcul (ModelValidator.cpp:576-578).

## BUG-155
Status: OPEN (MEDIUM, reproduit) — exceptions non contenues : prepare() (Fem3DError, Standard_ConstructionError) et 8 corps de
QThread::create sur 9 sans try.

## BUG-156
Status: OPEN (MEDIUM, reproduit) — poids propre : drapeau du cas ignoré en cas seul, case du calcul ignorée en combinaison,
selfWeightFactor ignoré par les trois moteurs.

## BUG-157
Status: OPEN (MEDIUM, code) — aucune sauvegarde automatique ni récupération après plantage.

## BUG-158
Status: OPEN (MEDIUM, mesuré) — Fem3D : 117 s contre 18 s pour OpenSees à 29 200 barres, environ 90 % dans SimplicialLDLT.

## BUG-159 à BUG-168
Status: OPEN (LOW) — saveToFile sans QGuiApplication ; OpenSeesPlanarSolver (TSALab) sans arrêt après délai, code 259 ; banc
Fem3D/OpenSees qui supprime ses références et conclut « 0 écart » sans comparaison ; moteur écrit en dur dans la note ; budget
Annuler sous-estimé ; TSA_OPS_SUCCESS non contrôlé ; version OpenSees non lue ; décompression sans plafond, CRC nul accepté ;
chiffrement XOR ; clé d'API en clair hors Windows.

## BUG-147
Status: OPEN (mineur, constat 2026-10-30) — DiagramGeometry::diagramTypeName renvoie des QStringLiteral français non
traduits (« Effort Tranchant (Vy) »…) : infobulles des puces du dock Diagrammes 2D et libellés 3D restent en français en
anglais / malgache. Piste : QCoreApplication::translate dans DiagramGeometry + traductions.

## BUG-144
Status: OPEN (constat, 2026-10-29) — secours OpenSees de Fem3D sans cas atteignable depuis l'interface : depuis
Fem3D 0.3.0 les charges sur barre sont calculées ; les seuls « hors périmètre » restants (treillis, câbles, coques)
sont refusés avant par AnalysisManager (familles d'éléments ; coques non transmises si !supportsShell). L'option
« secours OpenSees » ne change donc plus rien. Logique testée directement sur le moteur (test 478). Décision à
prendre : retirer l'option, ou faire passer ces familles par la décision du moteur quand un secours est choisi.
Espace de travail (2026-10-29, essais réels dans TSA.exe, branche feature/blueprint-workspace) :

## BUG-139
Status: OPEN — vue partagée à la taille minimale de la fenêtre (960 × 560 logiques) avec les panneaux gauche et droit
ouverts : zone centrale plus étroite que les minimums vue 3D (240 px) + canevas (280 px) ; le Blueprint est rogné.
Contournement : mode Blueprint, mode focus (Ctrl+Shift+F) ou fermer une colonne de panneaux. Piste : replier palette et
inspecteur de l'éditeur sous un seuil, ou relever la largeur minimale du workspace en vue partagée.

## BUG-140
Status: OPEN (ergonomie, décision à prendre) — Ctrl+Z dans le canevas du Blueprint annule le PROJET (modèle structurel),
pas l'édition du graphe (boutons propres à l'éditeur). Cohérent avec la documentation, peut surprendre en éditant un graphe.

## BUG-141
Status: OPEN (mineur) — mini-carte peu lisible pour un graphe très allongé (une rangée de 400 nœuds : bande quasi
invisible) ; « Cadrer tout » plafonné à 5 % : un tel graphe n'entre pas entier dans la vue.

## BUG-143
Status: OPEN (tests, inspection du code, non reproduit) — risques de blocage : boîtes modales sans répondeur (suite
mesh 390 / 299), OpenSeesSolver sans délai, wait() sans délai des contrôleurs ; TSA_TestSuite sans délai interne et
écrivant dans le dossier courant (fichiers test_bug110_*.tsa, empty_project.tsa à la racine du dépôt). docs/TESTING.md.

Validation Windows du 2026-10-28 (docs/reports/WINDOWS_VALIDATION_REPORT.md, identifiants W-n du rapport) :

## BUG-133 (W-3)
Area: OpenSees — téléchargement à l'exécution
Problem: OpenSeesManager::officialDownloadUrl en 404 (opensees.berkeley.edu redirige vers peer.berkeley.edu/openseesOpenSees/…,
défaut du serveur, constaté le 2026-10-28) ; downloadAndInstall ne vérifie pas l'empreinte (winget : 5E37A5A9…FB41).
Impact: HIGH pour le calcul OpenSees sur un poste neuf
Status: OPEN

## BUG-133 (W-3) — mise à jour 2026-10-28
Source officielle de l'archive Windows disparue (404). Contournement : OpenSees 3.8.0 conda-forge épinglé
(cmake/SetupDependencies.cmake, docs/DEPENDENCIES.md §5). Status: PARTIEL — installation vérifiée, exécution NON
validée ; OpenSeesManager::downloadAndInstall (application) vise encore l'adresse officielle.

RÉSOLU 2026-10-29 (soir) : cause = libblas.dll / liblapack.dll de conda-forge ne font que TRANSFÉRER leurs exports à
openblas.dll (paquet libopenblas), invisible pour dumpbin /dependents → 0xC0000135 au chargement. openblas.dll ajouté
(TSA_OPENSEES_CONDA_PACKAGES). Vérifié : commande de contrôle → OPENSEES_CHECK_OK_3.8.0 ; 57/57 suites (dont les 15
OpenSees) ; banc OpenSees 88/88 ; TSALab 27/27 sans partie ignorée. Status: FIXED (exécutable) ; OpenSeesManager::
downloadAndInstall (application) vise encore l'adresse officielle (échec propre).
Historique : Mise à jour 2026-10-29 : avec thirdparty/OpenSees (build conda-forge), la suite opensees échoue à
« mgr.isAvailable() » (test_opensees.cpp:133) : le contrôle de l'exécutable (OpenSeesManager::checkExecutable) ne le
déclare pas opérationnel. Cause NON déterminée : l'exécution directe d'OpenSees.exe pour lire sa sortie est refusée par le
contrôle de sécurité de la session. Pistes [H] : Tcl (tcl86t.dll cherche lib/tcl8.6 à côté de l'exécutable : présent),
DLL d'exécution, délai de 5 s du contrôle. À diagnostiquer en lançant le script de contrôle à la main.
## BUG-138
Area: SDK — tsa3d_roundtrip_example
Problem: l'exemple est produit dans <build>/sdk sans les DLL (Qt, OCCT) : lancé depuis un autre dossier courant, il échoue
(0xC0000135, DLL introuvable) ; il fonctionne depuis le dossier de compilation (dossier courant dans la recherche des DLL).
Constaté le 2026-10-29 en validant B ; antérieur à B (RUNTIME_OUTPUT_DIRECTORY sdk/ inchangé).
Impact: LOW (exemple de développement, non livré)
Status: OPEN

## BUG-137 (dépendances, audit 2026-10-28)
Area: cmake/ (presets, détection, déploiement)
Problem: Qt figé 6.11.2 dans les presets (W-9) ; plancher Qt 6.2 faux pour les traductions (qt_add_lrelease exige 6.9) ;
windeployqt en cache d'une autre Qt possible ; DLL OCCT déployées depuis un chemin figé ; aucun contrôle de chaîne de
compilation / mélange dans CMake.
Status: FIXED sous Windows (matrice m1-m7, docs/DEPENDENCIES.md) ; Linux NON TESTÉ ; TSALab sans détection auto de Qt
(appel tsa_find_qt à ajouter dans TSALab, accord requis).

## BUG-135 (W-6, W-7, W-8)
Area: chemins UTF-8 restants, déploiement
Problem: TSALab lab/Tests/LabTests.cpp:120 (L5, même défaut que BUG-131 : 17/27, 27/27 avec TEMP ASCII) ;
test_torsion.cpp:724 (remove sur chemin étroit, fichier laissé dans TEMP) ; DeployDependencies.cmake:30 liste Qt6* comme
« introuvables » malgré EXCLUDE_REGEXES (cosmétique).
Status: PARTIEL (2026-10-28) — W-8 FIXED : DeployDependencies.cmake retire Qt6* de la liste des introuvables
(filtre d'abord placé dans RuntimeDependencies.cmake, déplacé : il affaiblissait le contrôle strict du paquet ;
vérifié : plus d'avertissement, package_TSA + verify_package OK). W-7 corrigé
(TSA::Core::utf8Path, compilé) mais NON EXÉCUTÉ : la suite torsion s'arrête au test 321 sans OpenSees. W-6 OPEN
(TSALab : autre dépôt, correctif d'une ligne à autoriser).

Défauts BUG-125 à BUG-130 : constatés et FIGÉS par la suite « undo » (tests 513-523, branche test/etape2-caracterisation,
non commitée) ; non corrigés volontairement (caractérisation avant l'étape 2). Une correction devra modifier le test
marqué « DÉFAUT FIGÉ » explicitement.

## BUG-130
Area: Undo/Redo — défauts mineurs caractérisés
Problem: entrées créées sans changement (gabarit de bibliothèque vide, LibraryManager.cpp:329 ; proposition IA sans id,
AIToolRegistry.cpp:392 ; charge nodale validée sans contrôle, LoadPropertiesView.cpp:339) ; appui posé sans NodeModified
(NewNodeDialog.cpp:190) ; GridSettingsDialog sans vue 3D : duplication hors historique (GridSettingsDialog.cpp:394-396) ;
grilles et charges de surface de MainWindow_Mesh (326, 359) sans notification d'observateur ; validation de la grille
active sans changement : xIsBold / yIsBold / zIsBold normalisés (vides → false), sans effet visible (test 522).
Impact: LOW
Status: FIXED (2026-10-29) sauf un point cosmétique — corrigés : entrées sans changement (bibliothèque, IA, charge nodale),
appui posé sans NodeModified, appuis linéiques notifiés, duplication de grille sans vue 3D (GridSettingsDialog::setModel),
charges surfaciques notifiées (IModelObserver::onSurfaceLoadsChanged / Model::notifySurfaceLoadsChanged). Reste : gras de
grille normalisés à la validation (sans effet visible). Tests 514-523.

## BUG-124
Area: TSALab — suite complète sans OpenSees
Problem: sans OpenSees, la suite TSALab reste en échec : L10 (« calcul OpenSees ») exige OpenSees et, la suite
s'arrêtant au premier échec, empêche l'exécution des tests suivants (6 / 27 dans le rapport de correction de BUG-121 (4),
apres_sans.log). D'autres tests sont suspectés, non confirmés (non atteints) ; pistes d'investigation seulement, pas des
causes démontrées : LabTests.cpp, boucle sur custom2d/opensees (vers la ligne 462), réglages opensees (vers 558-583),
L14 « OpenSees disponible » (vers 694). Distinct de BUG-121 (4) (L9 seul).
Impact: TSALab ne peut pas terminer sa suite sur un poste ou une CI sans OpenSees.
Cause démontrée (diagnostic du 2026-10-26) : (1) main : (structuralOnly || runSuite_Lab) && runSuite_Structural — un échec de
la suite Lab empêchait l'exécution des 16 tests Structural (L15-L30), pourtant indépendants d'OpenSees (16/16 avec
--structural) ; (2) L10 à L14 exigeaient OpenSees sans contrôler sa disponibilité (L10 : calcul du portique spatial ; L11 :
passage opensees de la boucle ; L12 : extraction avancée et mode léger ; L13 : chaîne FEA-01/FEA-07 ; L14 : calcul des coques).
Correction (TSALab/lab/Tests/LabTests.cpp seul, branche fix/l9-sans-opensees, avec BUG-121 (4)) : les deux suites
s'exécutent toujours ; disponibilité d'OpenSees lue dans le registre des moteurs (availability(), jamais le texte d'une
erreur) ; seules les parties dépendantes d'OpenSees sont ignorées, avec une ligne [SKIP] et le motif du moteur ; compteur
« parties ignorées » dans le bilan RESULTS. Résultats Linux Release consignés dans le rapport de correction : sans OpenSees
27/27 réussis, code de sortie 0, six parties ignorées (L9 à L14) ; avec OpenSees 27/27, aucune partie ignorée (sortie
identique à la référence) ; un faux moteur disponible qui échoue au calcul n'est pas traité comme indisponible (L9 à L14
en échec, aucun [SKIP]) ; les erreurs numériques font toujours échouer les tests (RDM-01, FEA-01 faussés) ; les suites Lab
et Structural sont toutes deux exécutées ; échecs numériques et échecs de calcul toujours détectés. Commit TSALab local
58063a70e9d54fc461f85d1fe45acaa2c277ab5f (branche fix/l9-sans-opensees, lab/Tests/LabTests.cpp seul), non poussé.
Restent à valider : Windows/MSVC et poste sans Gmsh.
Preuves : TSA-rapports/2026-10-26/preuves/BUG-124/.
Status: CORRIGÉ LOCALEMENT — validation Linux effectuée — commit TSALab créé — validation Windows en attente.

## BUG-123
Area: Tests des raccourcis sous Windows (CI)
Problem: test 261 échoué une fois (CI 40292062083, tentative 1), réussi à la relance du même commit (tentative 2) et dans
les trois exécutions Windows précédentes : intermittent. Suite arrêtée au premier échec (262-266 non exécutés).
Cause probable (non prouvée, la condition ne dit pas quel terme échoue) : writeText du test (QSaveFile::commit, sans
nouvelle tentative) refusé quand l'antivirus ou l'indexeur tient shortcut.txt — phénomène documenté dans
ShortcutManager::writeFile, qui, lui, réessaie (12 × 50 ms).
Impact: LOW — test seulement ; aucune modification de code proposée sans accord.
Status: OPEN. 2026-11-01 : test 262 (« fichier remplacé une deuxième fois : toujours surveillé ») échoué une fois en suite
complète (Release, branche fix/no-wheel-on-fields), 5/5 réussi en suite seule : même famille (écriture de shortcut.txt).

## BUG-121
Area: Panneaux Propriétés (essais d'interface du 2026-10-26)
Problem: (1) CORRIGÉ : liste « Type de profilé » désactivée affichant un type faux pour une forme hors liste (UPN montré
« Profilé en I » ou « Rectangulaire ») → désignation réelle affichée (Beam/ColumnPropertiesView, test 493 complété).
(2) OUVERT : libellés des champs masqués (« Largeur (b) : », « Rayon (r) : »…) visibles sans champ (QFormLayout) ;
« Rayon (r) : » (cercle) voisin de « Rayon de congé (r) : ». (3) OUVERT : « IA non configurée » (barre d'état) non traduit.
(4) TSALab test L9 échouait sans OpenSees au lieu de signaler la partie OpenSees non exécutée. Cause (dans L9 seul) :
l'assertion exigeait que toutes les vérifications réussissent (16/16), OpenSees comprises ; l'en-tête exigeait
« OpenSees 3.8 » ; le bloc des références externes forçait le moteur opensees. La logique du banc était juste et n'a pas
été modifiée (moteur indisponible → cas skipped avec motif, vérifications non disponibles, jamais en échec), mais
checksUnavailable() mêle cas ignorés et calculs échoués d'un moteur présent : c'est au test de les distinguer.
Correction locale (TSALab, branche fix/l9-sans-opensees, lab/Tests/LabTests.cpp seul) : disponibilité d'OpenSees lue
dans le registre des moteurs ; sans OpenSees, cas OpenSees exigés ignorés (non exécutés, motif « indisponible », aucune
vérification disponible) avec une ligne [SKIP] ; tout autre cas exécuté avec toutes ses vérifications réussies ; version
jamais inventée (« OpenSees (version inconnue) ») ; références externes sur custom2d sans OpenSees. Comportement avec
OpenSees inchangé. Résultats Linux Release consignés dans le rapport de correction du 2026-10-26 : A (sans OpenSees) L9
réussi, 8 vérifications Custom2D réussies, 8 OpenSees non exécutées ; B (avec OpenSees) L9 16/16, TSALab 27/27 ;
C1 et C2 (valeur attendue faussée, sans et avec OpenSees) et C4 (OpenSees disponible, calcul en échec) : L9 en échec,
vrais échecs détectés ; C3 (exécutable qui plante à la sonde) : partie ignorée avec le motif déclaré par le moteur.
Preuves : TSA-rapports/2026-10-26/preuves/BUG-121_4/. Windows/MSVC non testé. Commit TSALab local
58063a70e9d54fc461f85d1fe45acaa2c277ab5f (branche fix/l9-sans-opensees, lab/Tests/LabTests.cpp seul, « fix(tsalab): tests
du laboratoire sans OpenSees (BUG-121 (4), BUG-124) »), non poussé.
Statut (4) : CORRIGÉ LOCALEMENT — validation Linux effectuée — commit TSALab créé — validation Windows en attente.
Status: PARTIAL.

## BUG-117
Area: Unités — consommateurs qui supposent kN (audit transversal du 2026-10-26, rapport hors dépôt)
Problem: P1 : raideurs d'appui exportées en kN/m dans un IFC qui déclare N (IfcExporter.cpp:132-139, 187, 423-430),
import sans facteur (IfcImporter.cpp:489-497) — VÉRIFIÉ ; comparaison Fem3D / OpenSees si OpenSees en N
(Fem3DEngine.cpp:195-300) ; export TSA3D des réactions (Tsa3dExport.cpp:376-387). P2 : affichages « kN » en dur
(résumé du dock, console, propriétés du nœud, flèches 3D, légendes des diagrammes : diagramUnit jamais renseigné,
coques) ; libellés faux (NDCGenerator « Résidu <= 0.001 kN » relatif ; moment réparti kN·m/m). P3 : équilibre Custom2D
par construction, g = 9,81 / 9,80665 (Beam::weight en N), UnitSystem : toute chaîne ≠ « N » vaut kN.
Impact: HIGH pour l'IFC (échange avec d'autres logiciels), MEDIUM ailleurs (OpenSees en N seulement).
Status: PARTIELLEMENT FIXED (2026-10-26, D4 approuvé) — P1 corrigés, reproduits avant correction : IFC (raideurs en N/m,
IfcDerivedUnit LINEARSTIFFNESSUNIT / ROTATIONALSTIFFNESSUNIT déclarées ; import : unité déclarée, sinon FORCEUNIT /
LENGTHUNIT / PLANEANGLEUNIT ; ancien export TSA reconnu par « … STEP writer » sans unité → kN/m + avertissement ; test 501,
lecteur indépendant IfcOpenShell 0.9 Python : 5000 N/m avant, 5e6 N/m après, 0 erreur de schéma) ; Fem3D / OpenSees en N
(166/240 faux écarts avant, 0 après ; test 499) ; TSA3D réactions en kN + bloc results.units (test 500, schéma mis à
jour). Références avant / après : TSA-rapports/2026-10-26/references/unites_echanges/. RESTE OPEN : P2 (affichages « kN »
en dur, diagramUnit), P3.

## BUG-118
Area: Paquets .tsalib — suite de BUG-109 (conception du 2026-10-26, hors dépôt)
Problem: manifeste extrait ≠ manifeste validé (C4) — CORRIGÉ le 2026-10-26 (empreinte de manifest.json = en-tête, test
496) ; flux non contrôlé — CORRIGÉ (stream.status(), en-tête relu comparé) ; restent : extraction non atomique, masquage
d'une extension (registre indexé par id de fiche, recherche approximative), ref.libraryId jamais renseigné (déchargement
sans effet), appartenance par préfixe, import de dossier sans approbation, minimum_tsa_version non vérifié, SNAP
(TSAFile.cpp:357-392) qui écrit org.tsaraloha.tsalib 1.0.0 pour toute fiche, « signé » annoncé à tort (ExtensionPackager.h:45).
Status: OPEN — étape E0 FAITE le 2026-10-26 (D3) : copie du paquet en préparation (<parent>/.staging/<uuid>), inspection et
extraction de la copie, création exclusive des fichiers, un seul renommage final ; refus si la cible n'est pas vide (pas de
mise à jour en place), si l'id est celui d'une extension livrée, si minimum_tsa_version (déclarée) > version de l'hôte ;
inspection : octets en trop, doublons de casse, noms réservés Windows, point / espace final, caractères interdits ;
découverte : le premier dossier (livré) l'emporte, conflit dans discoveryWarnings() ; import de dossier réservé au mode
développeur ; commentaire « signé » retiré. Test 505. RESTE : E1 (registre qualifié, provenance SNAP), E2 (magasin
versionné, journal, reprise, retour arrière), E3 (minisign/libsodium), E4, E5.

## BUG-111
Area: Traçabilité des calculs (ResultsModel, OpenSeesSolver, NDC, NationalAnnexConfig)
Problem: version de moteur inventée par défaut (ResultsModel.h:197-198 « OpenSees 3.8.0 », OpenSeesSolver.cpp:421), aucune
empreinte de l'entrée, script Tcl détruit (QTemporaryDir), chunk RSLT déclaré mais jamais écrit, annexe nationale et γ/ψ non
figés dans le projet (singleton), NDC historique qui lit la version OpenSees du jour (NDCGenerator.cpp:127).
Impact: HIGH (traçabilité, NDC régénérée différente). Status: OPEN — étape 0 FAITE le 2026-10-26 (D2) : métadonnées
neuves vides (plus de « OpenSees 3.8.0 » / « France NF » par défaut), aucune version de repli, annexe non attribuée au
calcul, configuration réelle du script (BasicBuilder, RCM), horodatage UTC, messages de validation avant calcul conservés
(preparationMessages) ; NDC : moteur et version des résultats (« version non lue » sinon), plus « certifiées », annexe des
vérifications EC2/EC3 indiquée à part. Tests 503, 504 ; test Standards.8 adapté (il vérifiait les valeurs inventées).
RESTE : étapes 1-2 (empreintes, AnalysisTrace en mémoire), 3+ (format 1.14) après validation.
Related files: src/Analysis/ResultsModel.h, src/Analysis/OpenSeesSolver.cpp, src/NDC/NDCGenerator.cpp, src/Standards/NationalAnnexConfig.*

## BUG-115
Area: Panneau Propriétés du poteau — « Tube Rectangulaire / Creux » (ColumnPropertiesView::getSectionFromUi)
Problem: choisir explicitement « Tube » produit un rectangle PLEIN nommé « Poteau Rectangulaire » (aucun champ
d'épaisseur). Depuis BUG-106, ce n'est plus déclenché par une autre modification, seulement par ce choix explicite.
Impact: MEDIUM. Status: OPEN.

## BUG-104
Area: Tests — i18n, test 457 (recherche, seuil 50 ms)
Problem: en Debug, la requête la plus lente dépasse le seuil (60 ms sur 6 000 entrées ; ~11 ms en Release) ; l'échec
arrête la suite, et les tests 458–463 ne s'exécutent pas (décompte Debug 482/490 au lieu de 489).
Reproduction: build-linux-debug, 2026-10-25, suite complète xcb.
Impact: LOW (performance réelle conforme en Release) ; masque toutefois 6 tests en Debug.
Status: OPEN — pistes : seuil selon NDEBUG, ou poursuivre la suite après cet échec.
Related files: tests/test_i18n.cpp, tests/test_main.cpp

## BUG-102
Area: OpenSees — chemin explicite de l'exécutable (src/Analysis/OpenSeesManager.cpp)
Problem: aucune option de l'interface n'écrit le chemin d'OpenSees : `setCustomExecutablePath` n'a aucun appelant ;
seule la clé QSettings `OpenSees/ExecutablePath` (organisation « TSA », application « TSA_StructuralAnalysis » ;
~/.config/TSA/TSA_StructuralAnalysis.conf sous Linux, registre sous Windows) permet de l'imposer, à la main.
Reproduction: Grep `setCustomExecutablePath` dans src/ (2026-10-25) : seule la définition.
Impact: LOW sous Windows (exécutable livré dans thirdparty/OpenSees), MEDIUM sous Linux (application lancée depuis le
bureau : OpenSees introuvable si ~/.profile n'ajoute pas son dossier au PATH).
Status: OPEN — comportement vérifié le 2026-10-25 (suite opensees) : chemin enregistré prioritaire, PATH, environnement
minimal (introuvable, message explicite). Ne pas proposer de lien symbolique (Tcl ne trouve plus lib/tcl8.6).
Related files: src/Analysis/OpenSeesManager.cpp, docs/LINUX_BUILD.md §7

## BUG-076
Area: Tests / intégration continue (vue 3D OCCT)
Problem: toute suite qui crée une `OccView` s'arrête (code 0xC0000409, `abort()` du CRT, sans message) sous la
plateforme Qt `offscreen` : la fenêtre n'a pas de vrai HWND, OCCT (`WNT_Window`, contexte WGL) ne peut créer son
contexte OpenGL. Le workflow de publication (Setup-Livrable, `QT_QPA_PLATFORM: offscreen` à l'étape Tests) fait donc
échouer `display`, `audit`, `multidoc` et `TSA_AllTests` — ce n'était pas un défaut du code des suites.
Reproduction: `set QT_QPA_PLATFORM=offscreen` puis `TSA_TestSuite.exe --suite=display` → 0xC0000409 dès la première
vue ; avec `QT_QPA_PLATFORM=windows` (vraie fenêtre) : display 6/6, audit 4/4, multidoc 10/10 (17/10/2026, build-ci
propre). Équivalent local de la CI (offscreen) : 26/30 tests CTest réussis, exactement ces quatre en échec. Même résultat avec le binaire
antérieur aux travaux IFC (TSA_TestSuite du 17/10 13:54) : défaut préexistant, indépendant d'IFC.
Impact: HIGH pour la publication (étape Tests de la CI en échec), aucun impact pour l'utilisateur.
Status: OPEN — piste : exécuter ces suites avec la plateforme `windows` sur le runner (session de bureau des runners
GitHub Windows ; OpenGL logiciel à vérifier : Mesa llvmpipe si le pilote Microsoft ne fournit que GL 1.1), ou les
annoncer sautées sous `offscreen` comme le test 408 de la suite `ifc`. À valider par une exécution réelle de la CI.
18/10/2026 — correctif préparé dans Setup-Livrable (branche locale `feature/tests-vue3d`, non publiée) :
`scripts/run_tests.ps1` en deux passes ; passe `offscreen` sans les suites à vue 3D, passe en plateforme `windows`
pour AllTests, audit, display, multidoc, drawing et ifc (408 et 419 réellement exécutés), avec le rendu OpenGL
logiciel de Mesa 26.2.3 (llvmpipe, empreinte figée) posé à côté de l'exécutable des tests puis retiré. Échec explicite
en session 0, si une suite attendue manque, si un test est sauté ou si le total de la suite complète est incomplet.
Essai local (build-ninja-release, 79dfdea) : offscreen 25/25 ; windows + Mesa 6/6, suite complète 427/427, rendu
« Mesa — llvmpipe, OpenGL 4.6 » (test 370), aucun [SKIP]. Garde-fous éprouvés sur de fausses suites CTest (test
sauté avec CTest « réussi », arrêt 0xC0000409, suite absente : échec dans les trois cas). Reste à vérifier sur le
runner : session interactive (session ≠ 0) et durée de la passe native.
24/10/2026 — même défaut sous Linux : `QT_QPA_PLATFORM=offscreen` → « X Error … BadWindow » et arrêt (suites
display, multidoc, meshview après le test 343, désormais [SKIP] comme 408). scripts/linux/test.sh choisit « xcb » quand
un affichage X11 et le plugin xcb de Qt (libxcb-cursor0) sont disponibles.
Related files: tests/test_display.cpp, tests/test_audit.cpp, tests/test_multidoc*.cpp, Setup-Livrable/.github/workflows/release.yml, Setup-Livrable/scripts/run_tests.ps1

## BUG-079
Area: Tests / mesure de temps (test 20, suite `commands`)
Problem: après BUG-035 (meilleur de 3 annulations, seuil de 350 ms inchangé), le test 20 échoue encore de temps en
temps quand la machine est chargée : deux échecs sur une dizaine d'exécutions complètes le 18/10/2026, chaque fois
pendant une compilation ou une autre suite en parallèle ; il repasse seul et dans les exécutions suivantes (suite
complète native 427/427 à 07:49 le 18/10/2026).
Impact: MEDIUM pour la CI (échec possible sur un runner partagé), aucun pour l'utilisateur.
Status: OPEN — seuil volontairement non modifié. Pistes à évaluer avant toute décision : mesurer le temps CPU du
processus plutôt que le temps écoulé, ou isoler les mesures de performance dans une suite à part lancée sans
parallélisme ; ne pas assouplir le seuil pour faire passer la CI.
Related files: tests/test_commands.cpp

## BUG-077
Area: Import IFC / titre de la fenêtre
Problem: le projet issu d'un import IFC s'ouvre dans un nouvel onglet nommé d'après le fichier, mais la barre de titre
affiche « TSA — Sans titre * » tant qu'il n'est pas enregistré (`ProjectManager::windowTitle` ne connaît que le nom du
fichier).
Reproduction: Importer IFC… → ifc4x3_Building-Structural.ifc → onglet « ifc4x3_Building-Structural ● », titre « Sans titre ».
Impact: LOW (cosmétique ; l'onglet et l'arbre portent le bon nom)
Status: OPEN — constaté le 17/10/2026 (essai UI Automation de l'import IFC).
Related files: src/Project/ProjectManager.cpp, src/UI/MainWindow_Bim.cpp

## BUG-075
Area: Sélection 3D / nœuds
Problem: un clic simple sur un nœud dans la vue 3D ne le sélectionne pas (Propriétés vides, « 0 sél. »), avec ou
sans cotation ; la sélection par l'arbre fonctionne. Hypothèse non vérifiée : l'objet détecté en premier sous le
curseur est un axe de grille ou une étiquette (non enregistrés), d'où `clearSelection`.
Reproduction: qa_topo.tsa, vue 3D, clic sur le nœud N8 → infobulle « Nœud N8 », aucun nœud sélectionné.
Impact: MEDIUM (sélection des nœuds à la souris)
Status: OPEN — constaté le 2026-10-10 pendant la vérification des cotations ; piste : étendre
`OccView::detectedPreferringModel` à tout objet détecté non enregistré.
Related files: src/Viewer/OccView_Events.cpp, src/Viewer/OccView_Shapes.cpp

## BUG-002
Area: Calculation
Problem: Dalles et voiles ne sont pas transmis au calcul (`CalculationSnapshot` /
`OpenSeesAnalysisBuilder` ne traitent que barres, treillis, câbles, appuis) ; aucun maillage EF.
Reproduction: modèle avec dalle chargée → la dalle n'apparaît pas dans le script Tcl.
Impact: HIGH (limitation fonctionnelle à signaler à l'utilisateur)
Status: OPEN — mitigé le 2026-10-04 : avertissement `ModelValidator::validateForAnalysis`, confirmation
avant calcul (`confirmPlanarElementsExcluded`, MainWindow_Tools.cpp), et
`onActionMeshGen` n'affirme plus avoir généré un maillage (il ne fait qu'une estimation).
Related files: src/Analysis/CalculationSnapshot.cpp, src/Analysis/OpenSeesAnalysisBuilder.cpp

## BUG-006
Area: Undo/Redo
Problem: 28 appels de production à `Model::pushUndoState` dans 20 fichiers (recompté le 2026-10-26 sur 39155c4 ;
l'ancien décompte « 37 », OccView_Events et MainWindow_Transform compris, est périmé : ces deux fichiers n'en ont plus).
Familles : F1 panneaux 10, F2 chargements 9, F3 appuis 3, F4 grilles 3, F5 création / bibliothèques 2, F6 IA 1. S'y
ajoutent 5 appels à `pushLabelUndoState` (Annotation/DimensionService.cpp ×4, Topology/TopologyNumberingService.cpp).
Fonctionnels, mais sans `EditRecord` sauf poutre, poteau (section, matériau) et nœud (coordonnées).
Impact: LOW
Status: OPEN
Notes: comportement figé AVANT toute migration vers EditTransaction par la suite « undo » (tests 513-523, branche
test/etape2-caracterisation, non commitée) ; 25 appels sur 28 atteints (MainWindow_Mesh.cpp:326, :359 et
MainWindow_Tools.cpp:464 sont dans la couche application, non liée aux tests). Défauts figés : BUG-125 à BUG-130.

## BUG-008
Area: Viewport / Performance
Problem: Ouverture d'un modèle de 4 896 barres ≈ 16 s en Debug (reconstruction 3D ≈ 13 s : une
sphère + un label par nœud, un solide par barre). Non mesuré en Release.
Impact: LOW
Status: OPEN
Notes: piste : partager la géométrie des sphères de nœuds (TopLoc_Location).

## BUG-012
Area: Environment
Problem: Processus TSA.exe « clones WER » bloqués (anciens crashs) qui verrouillent l'exécutable
(LNK1168) ; non terminables (accès refusé).
Impact: LOW
Status: OPEN — contournement : renommer TSA.exe ; disparaissent au redémarrage.

## BUG-021
Area: AI / performance
Problem: sur le poste de développement (AutoCAD, Revit, Chrome ouverts, ≈3 Go de RAM libre), Qwen3 4B
Q5_K_M tombe à ≈1,3 jeton/s (CPU comme GPU) : pages du modèle évincées. Le chargement en `--no-mmap`
ne termine pas en 4 min. Le sélecteur en tient compte (RAM libre mesurée) et recommande alors un
modèle compact ; le diagnostic signale l'écart mesure/estimation.
Impact: MEDIUM (qualité des réponses limitée sur machine chargée)
Status: OPEN (contournement : fermer les applications lourdes ; modèle plus léger)

## BUG-022
Area: IO / compatibilité
Problem: une version de TSA antérieure au format 1.2 ne peut pas ouvrir un fichier 1.2 (bloc d'aperçu lu
comme payload → CRC invalide). Lecture des anciens formats par la version courante : OK (test 127).
Impact: MEDIUM si des versions antérieures sont distribuées ; nul en développement.
Status: OPEN (documenté, ADR-016)

## BUG-023
Area: Platform / Explorateur
Problem: après désinstallation de l'extension, les miniatures déjà en cache Windows (thumbcache) restent
visibles tant que les fichiers ne changent pas. Constaté via IShellItemImageFactory.
Impact: LOW (comportement Windows ; Nettoyage de disque › Miniatures)
Status: OPEN (documenté)

## BUG-025
Area: Tests GUI (UI Automation)
Problem: un `Invoke` UIA sur une action qui ouvre une fenêtre modale (`exec()`) ne rend pas la main et bloque les
requêtes UIA suivantes vers TSA (timeouts) ; l'application reste réactive.
Notes: vérification de la fenêtre Analysis faite par capture d'écran (Invoke lancé dans un job séparé).
Impact: LOW (outillage de test)
Status: OPEN

## BUG-028
Area: BIM / IFC
Problem: export IFC partiel — charges / cas / combinaisons, relâchements d'extrémité, excentrements, appuis orientés et
grilles non exportés ; unités dérivées (module, masse volumique) non déclarées ; GlobalId des relations et Psets
recréés à chaque export ; fondations sans objet analytique IFC. Liste tenue à jour dans docs/IFC_MAPPING.md §4.
Impact: MEDIUM (échange incomplet, pas de perte dans le .tsa)
Status: OPEN
Related files: src/BIM/IFC/IfcExporter.cpp, docs/IFC_MAPPING.md

## BUG-033
Area: UI / Fenêtre
Problem: non vérifiable sur le poste de test (un seul écran à 125 %, Windows 10) : DPI différent par écran et
Windows 11 « Snap layouts » au survol du bouton Agrandir (HTMAXBUTTON non renvoyé : le renvoyer exigerait de gérer
les clics non clients du bouton). Le redimensionnement par les bords couverts par les fenêtres natives du workspace
est corrigé (voir FIX-2026-10-06-RESIZE).
Impact: LOW
Status: OPEN (à vérifier sur Windows 11 / multi-écrans)
Related files: src/UI/Shell/AppShell.cpp

## BUG-036
Area: Build (cache CMake local)
Problem: le 2026-10-08, `build-ninja-debug/CMakeCache.txt` avait `CMAKE_CXX_FLAGS` (et `_DEBUG`…) vides : compilation
sans /EHsc ni /Zi /Od (avertissements C4530) et test 96 en échec (destructeurs non appelés pendant le déroulement d'une
exception → pas de rollback automatique de l'EditTransaction). Cause exacte non identifiée (une configuration lancée
alors que vcvars64.bat signalait « vswhere.exe non reconnu » est suspecte).
Impact: MEDIUM (comportement des exceptions faux, sans erreur de compilation)
Status: WORKAROUND — `cmake --preset ninja-debug --fresh` rétablit les drapeaux (212/212 vérifié). Contrôle rapide :
`Select-String build-ninja-debug\CMakeCache.txt -Pattern "^CMAKE_CXX_FLAGS:"` doit contenir /EHsc.
