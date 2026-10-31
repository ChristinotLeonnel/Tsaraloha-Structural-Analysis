# Current State

Last Updated: 2026-10-31 — branche refactor/linear-only (analyse non linéaire retirée, ADR-025, non commitée). Sections non
revues depuis le 2026-10-07 : voir l'audit du 2026-10-31 (TSA_CODE_QUALITY_AUDIT.md, § Documentation).

Légende : IMPLEMENTED · PARTIAL · BROKEN · MISSING · UNKNOWN (preuve dans le code ou test exigée).

## Version linéaire seule (ADR-025, 2026-10-31)
Status: IMPLEMENTED (branche refactor/linear-only, non commitée) — TSA prend actuellement en charge l'analyse structurale
linéaire (statique linéaire, modale) ; l'analyse non linéaire n'est pas disponible. AnalysisType { LinearStatic, Modal } ;
script statique OpenSees unique (algorithm Linear, LoadControl 1.0, analyze 1, geomTransf Linear, treillis truss ;
câbles corotTruss + InitStrain conservés). Anciens projets non linéaires : ouverts, signalés, calcul refusé
(AnalysisContext::unsupportedAnalysisType, AnalysisEngine::unsupportedSettings, AnalysisManager::unsupportedFeatures).
Vérifié : 57/57 suites, 544 tests (532-537 nouveaux), 0 échec ; 104/104 fichiers de l'empreinte des exemples identiques à
l'octet (Tcl et résultats) ; bancs : 15 312 valeurs communes identiques au bit près ; banc officiel 1 682/1 682 (10 cas
ajoutés, RDM-11a/b supprimés) ; performances inchangées ; fuzzing de la voie de calcul rejoué : identique à l'audit ;
Debug (build-ninja-debug, première compilation) : 561 étapes, 0 erreur, 126 tests réussis sur 12 suites, test 457 en échec
(seuil de temps, BUG-169) ; TSALab 27/27 (test L9 : 36 cas du banc partagé, branche
refactor/linear-only de TSALab, non commitée). Rapport : docs/audit/2026-10-31/LINEAR_ONLY_REFACTORING_REPORT.md.

## Audit technique du 2026-10-31
Status: BROKEN pour OpenSees + rotule + charge non uniforme (BUG-148, toujours ouvert). BUG-149 (efforts le long des barres en
statique non linéaire) et BUG-150 : clos par retrait de la fonctionnalité (ADR-025). Statique linéaire 3D sans rotule : VALIDÉ contre un solveur indépendant (écart ≈ 1e-10, OpenSees et Fem3D).
Rapport : docs/audit/2026-10-31/TSA_AUDIT_EXECUTIVE_SUMMARY.md. Sections de ce fichier datées à relire : Meshing, Import/Export,
.tsa, Structural Calculation (TSA_CODE_QUALITY_AUDIT.md, § Documentation).

## Écosystème Tsaraloha (ADR-024, docs/TSARALOHA_ARCHITECTURE.md)
Status: IMPLEMENTED (phases 1 à 6, 2026-10-08, branche feature/tsaraloha-shared-libs) — bibliothèques partagées
Model / Graphics / Widgets, ProjectSession, SelectionSynchronizer, EcosystemApplication, cœur scientifique
tsalab_science (dépôt TSALab), registre de commandes. TSALab.exe = IDE propre sur ces bibliothèques.
Blueprint : moteur + éditeur partagés (docs/BLUEPRINT.md), vérifié dans TSALab (exemple portique exécuté).
Phase 7 IMPLEMENTED : AnalysisController dans ProjectSession (MainWindow n'orchestre plus le calcul),
AnalysisManagerPanel partagé, portée « plan du modèle », export K·U = F par Custom2D (dock Données d'analyse,
onglet K·U = F ; SOLVER LAB), commandes analysis.run / results.summary / results.node_displacement.
Vérifié : tests 196 (TSA), L7 (TSALab), GUI TSALab (portique : calcul, synthèse, SOLVER LAB LU / Cholesky / GC).
Phase 8 IMPLEMENTED (2026-10-08) : débogueur Blueprint (points d'arrêt, pas à pas, valeurs, arrêt), exécution en
tâche de fond des graphes sans accès au projet, historique de l'éditeur, script de commandes ↔ Blueprint, outils IA
list_commands / propose_blueprint, API de plugins (docs/PLUGINS.md, PluginManager chargé par EcosystemApplication),
OpenSees dans tsalab_science (OpenSeesPlanarSolver, validation croisée 14/14). Tests 197–198 (TSA), L8 (TSALab), S6.
Sous-graphes Blueprint IMPLEMENTED (2026-10-17, branche feature/blueprint-structurel, fusionnée dans main) : moteur
(BlueprintSubgraph.*), fichier v3 et .tsbps, éditeur (encapsulation, corps ouvert, propriétés, import / export).
Vérifié : tests 392–393, 393/393 PASS (Debug et Release, 2026-10-17) ; essai de TSALab réel (UIA) : double-clic sur une
instance, fil d'Ariane, propriétés, retour. Import d'une définition présente au contenu différent :
remplacée par celle du fichier, avertissements, annulable.

## TSA Core — programme approuvé le 2026-10-25
Status: PARTIAL — évolution progressive (pas de réécriture) vers un noyau C++20 (rapport d'audit du 2026-10-25, hors dépôt).
Phase 0 (intégrité) en cours sur fix/core-integrity (non commitée) : BUG-105 à 109, 112 à 114, 116 corrigés ; BUG-117 P1
(unités IFC, Fem3D, TSA3D), BUG-111 étape 0, BUG-118 étape E0 faits le 2026-10-26 (tests 492–505). BUG-110 implémenté
(format 2.0, 2026-10-26, migration démontrée au bit près). BUG-119 (plantage llvmpipe) et BUG-120 ouverts. Release Linux 503/504 (test 128 Windows seul), banc 146/146, TSALab 27/27.
Non vérifié : Windows/MSVC, CI GitHub (aucun push), essai manuel de l'interface. Écart connu : calcul modal présent dans le code malgré ADR-022 (décision
à prendre, ne pas supprimer sans accord).

## Build
Poste au 2026-10-31 : Qt 6.12.0 réinstallée dans C:/Qt/6.12.0/msvc2022_64 (Qt retirée par une maintenance Qt le 30/10),
OpenSees 3.8.0 réinstallé (thirdparty/OpenSees + build-windows-validation/engines/OpenSees) — BUG-146. Release
build-windows-validation : 57/57 suites (opensees relancée seule après un délai au premier lancement), test 531 compris.
Status: IMPLEMENTED — PASS (preset ninja-debug, -j 4)
Compiler: MSVC 19.51 (VS 18 2026 Community), C++20
Qt: 6.11.2 · OpenCASCADE: 8.0.1 · CMake: 4.3.3 (min 3.20)
Warnings: aucun connu (C4996 corrigé, BUG-010).
Validation Windows 2026-10-28 (docs/reports/WINDOWS_VALIDATION_REPORT.md) : PARTIEL. Poste actuel = Qt 6.12.0 (6.11.2 absent,
presets inutilisables tels quels), CMake 4.5.0-rc1, profil non ASCII ; Release Ninja dans build-windows-validation : TSA et TSALab
compilés, 27 avertissements (dépréciations OCCT, masquages, [[nodiscard]] dans les tests), 42 suites réussies, Custom2D 58/58,
TSA.exe démarre. OpenSees absent du poste (thirdparty/OpenSees/bin mis à la Corbeille le 2026-10-28) : 14 suites non exécutées.
CMake télécharge désormais OpenSees s'il est absent (SHA-256 vérifié), mais le serveur officiel est en 404 (BUG-133).
Édition (2026-10-29, branche refactor/core-edit-api, NON fusionnée, NON poussée) : toute modification de l'interface passe par
EditTransaction (contrat exact, coalescence, hasChanges) ; BUG-125 à 129 corrigés ; Windows : 42 suites / 0 échec / 15 non
exécutées (OpenSees masqué), banc Custom2D 58/58, TSALab 27/27 ; Linux non vérifié.
Dépendances (2026-10-28, branche build/dependency-detection) : les presets n'imposent plus Qt ; tsa_find_qt choisit la Qt
compatible la plus récente ; rapport <build>/tsa-dependencies.txt ; politique de versions : docs/DEPENDENCIES.md.
Note: MSVC francisé → lanceur généré `build-*/msvc_codepage.cmd` (page de code de la génération imposée à cl) :
dépendances d'en-têtes Ninja fiables quelle que soit la console (BUG-011, vérifié `ninja -t deps`).

## Build Linux (2026-10-24, branche fix/debug-build-errors)
Status: IMPLEMENTED (Linux) — 2026-10-25 : OpenSees, Gmsh 4.15.2 (SDK, ~/opt/gmsh-4.15.2-Linux64-sdk) et IfcOpenShell
0.9.0post1 (~/opt/ifcopenshell-0.9.0post1) intégrés (BUG-095, 099–101, 103 corrigés). Suite complète xcb : Release
489/490 (0 échec, test 128 ignoré : Windows seulement) ; Debug 482/490 (test 457, seuil de performance, BUG-104).
Historique ci-dessous. Debug et Release compilés (presets linux-debug / linux-release, scripts/linux/, docs/LINUX_BUILD.md).
Mise à jour 2026-10-24 (soir) : interface lancée (Start Center, projet pont 253 nœuds / 665 barres, vue 3D, Mesa iGPU) ;
suite complète en xcb : Release 363/471 comptés, 503 PASS, 4 SKIP, 16 FAIL — tous OpenSees / Gmsh / IfcOpenShell absents
(BUG-095) ; Debug idem + test 457 (seuil de performance). BUG-094, 096, 097 corrigés.
Ubuntu 24.04, GCC 13.3, CMake 3.28, Ninja 1.11, Qt 6.12.0 gcc_64, OCCT 8.0.1 compilé depuis les sources
(~/opt/occt-8.0.1, 4 modules, RPATH $ORIGIN). Avertissements : dépréciations OCCT 8 seulement.
Tests (offscreen, suite par suite, 55 suites) : Debug 35 sans échec / 480 PASS / 22 SKIP ; Release 36 / 487 / 22.
Échecs : OpenSees / Gmsh / IfcOpenShell absents
(BUG-095), format de module .exe (BUG-094), i18n 457 en Debug seulement (seuil 50 ms ; 11 ms en Release) ; arrêt
BadWindow sous offscreen dans display, multidoc, meshview (BUG-076, vues OCCT réelles requises).
OpenSees 3.8.0 Linux (2026-10-24, ~/opt/opensees-3.8.0, Conan 2) : cas analytiques exacts ; suite complète Release
(xcb, OpenSees dans le PATH) 450/471 — 4 échecs, tous Gmsh / IfcOpenShell (400, maillage, 354, 367).
Non vérifié : compilation Windows (MSVC) des modifications (24 et 25/10), dont les #ifdef de Gmsh et OpenSees.

## Tests
Status: IMPLEMENTED — 544 réussis, 0 échec, 1 ignoré (57/57 suites, Release, bureau masqué) le 2026-10-31 après
retrait du non linéaire (+ 532-537) ; 538 avant (audit du 2026-10-31) ; 233/233 PASS le 2026-10-10 (208 sélection 3D, 209–212 charges sur barre) ; 228/228 PASS le 2026-10-09 (audit QA : 205 annulation, 206 résultats périmés, 207 JSON des grilles) ; 225/225 PASS le 2026-10-09 (202 repères de grille, 203–204 déformée continue) ; 222/222 PASS le 2026-10-09 (test 201 : chemins accentués, chunk > 64 Kio, nom du projet, ouverture atomique ; TSALab 5/5, science 6/6, banc 14/14) ; 219/219 PASS le 2026-10-08 (197 débogueur Blueprint, 198 script ↔ Blueprint + IA) ; 217/217 avant (196 contrôleur d'analyse) ; 216/216 avant (192 registre de commandes, 193–195 Blueprint) ; 212/212 PASS le 2026-10-07 (ajouts : 166 arbre indexé, 181–184 extraction exacte,
188 niveaux/grilles annulables, 189 chunk SETT, 190 collage BIM, 191 édition groupée) ; test 20 sensible au temps (BUG-035) ;
197/197 avant (suite `bim` 170–180 le 2026-10-06, dont IFC 177–180) ;
186/186 avant (suite `cleanup` 160–165 le 2026-10-05) ;
180/180 avant (suite `mdd` 150–159 le 2026-10-05) ;
170/170 avant (suite `tools` 140–149 et test 139 ajoutés le 2026-10-05) ;
159/159 avant (`TSA_TestSuite.exe` ; suite `engines` tests 130–138 ajoutée le 2026-10-05 ;
tests 101–111 ajoutés le 2026-10-04 ;
suite `extraction` = validation numérique contre le binaire OpenSees 3.8.0)
Non couvert : comportement GUI (OccView, docks, mode 2D) — vérifications manuelles uniquement.

## Fenêtre / Start Center (ADR-021)
Status: IMPLEMENTED (2026-10-06, branche feature/start-center) — `AppShell` + `TitleBar` + `StartCenter` +
`NewProjectDialog` ; workspace (`MainWindow`) créé au premier projet. Vérifié dans l'application (Debug, 125 %) :
lancement → Start Center seul (fenêtre < 1 s), Nouveau projet (dialogue, fichier créé, workspace, récents), Ouvrir
(Ctrl+O, stress_4900), projet récent (simple clic = sélection, double-clic = ouverture, vue restaurée), Fermer le projet
(prompt Enregistrer / Ne pas enregistrer / Annuler) → Start Center, fermeture code propre, ligne de commande
`TSA.exe fichier.tsa`, recherche / tri / « Retirer des projets récents » (fichier conservé), menu d'application,
thème clair/sombre, agrandir / restaurer / réduire / déplacer / double-clic / redimensionnement bords et coins,
Annuler / Rétablir de la barre de titre, redimensionnement par les bords en mode Workspace, calcul OpenSees depuis
l'interface (appuis via le menu Structure). Réduire depuis l'état agrandi : crash corrigé le 2026-10-07 (FIX-2026-10-07-MINIMIZE). Non vérifié : multi-écrans / DPI mixte, Snap layouts Win11 (BUG-033).

## UI
Espace de travail (2026-10-29, branche feature/blueprint-workspace) : IMPLEMENTED — WorkspaceView (src/UI/Workspace) autour
de la vue 3D : modes Modèle 3D / Blueprint / vue partagée (horizontale ou verticale), mode focus, disposition par défaut ;
onglet de ruban Blueprint, panneau Affichage › Espace de travail ; préférences QSettings « Workspace » sous
HKCU\Software\TSA Engineering\TSA (docs/UI.md). Éditeur : cadrages, zoom, mini-carte adaptative, grille, panneaux
repliables, recherche Ctrl+F (types ET nœuds placés, clavier : Entrée / Bas), scène étendue aux éléments.
Vérifié automatiquement : tests 524–529 ; 57/57 suites (528 tests, 149 s, run_test_suites.py), TSALab 27/27.
Vérifié dans TSA.exe réel (2026-10-29, pilotage UI Automation + captures, Windows 125 %) : lancement, Ctrl+Alt+1/2/3,
panneaux masqués / rétablis (onglet actif conservé), mode focus, disposition par défaut, fermeture puis relance (mode
et panneaux restaurés, démarrage en Blueprint), Blueprint → modèle (2 nœuds + 1 poutre) puis Annuler ×3 / Rétablir ×3,
graphe de 401 nœuds (≈ 0,5 s), recherche #250 au clavier, AltGr AZERTY et clavier US, fenêtre minimale.
Limites : vue partagée à la taille minimale avec les deux colonnes de panneaux = Blueprint écrasé (BUG-139) ;
Linux non compilé ; rendu OCCT masqué / réaffiché non couvert par un test automatique (vérifié à l'écran seulement).
Ruban : responsive sans scroll (Full → IconOnly → Collapsed → « Plus »), barre d'accès rapide dans la
rangée d'onglets, infobulles riches. Limite : `ThemeManager::ribbonScrollStyleSheet` devenu inutilisé ;
pas de commandes Wireframe/Shaded/Offset/Trim… (inexistantes dans TSA, donc non ajoutées) ; fenêtre < ~700 px non vérifiée.
Status: IMPLEMENTED — MainWindow + ruban + docks (arbre, visibilité, éléments, propriétés,
résultats, console, projection, diagrammes, NDC), WindowManager (layouts).

## Miniatures Explorateur
Status: IMPLEMENTED (2026-10-04) — TSAThumbnailProvider.dll + format 1.2. Vérifié par le Shell
(IShellItemImageFactory) et par tests 127–129. Non vérifié : redémarrage, HKLM, installeur.

## Projets récents / aperçus
Status: IMPLEMENTED (2026-10-04) — accueil « Projets récents », aperçus capturés dans le viewport réel,
cache + métadonnées, caméra restaurée. Vérifié dans l'application : cartes affichées avec les vrais
aperçus, recapture après changement de caméra, message « Dernière vue du projet restaurée ».
PARTIAL : état d'affichage (viewState) mémorisé mais non restauré. Voir docs/PROJECT_PREVIEWS.md.

## AI Co-Engineering
Status: IMPLEMENTED (première itération, 2026-10-04) — `src/AI` (matériel, registre/sélection, gestion
des modèles, fournisseurs, contexte, outils, vérification, RAG, orchestrateur) + `src/UI/AI` (panneau,
configuration non modale, indicateur barre d'état, ruban Outils/Analyse, menu contextuel de l'arbre).
Vérifié dans l'application (UI Automation) : détection matérielle réelle, téléchargement + SHA-256,
démarrage llama-server, diagnostic, auto-benchmark (CPU 40,8 / Vulkan 4,4 j/s), « Vérifier la
structure » et question libre (réponses fondées sur les données du .tsa). Tests 112–123.
Fournisseur Cloud « Anthropic Claude » ajouté le 2026-10-09 (préréglage `anthropic`, point d'accès
« compatible OpenAI » officiel d'Anthropic, même `OpenAICompatibleProvider` que OpenAI/Gemini/Ollama,
aucun code propre à Anthropic) ; préréglage vérifié dans la boîte de dialogue (UI Automation), jamais
appelé en vrai (nécessite une clé API Anthropic de l'utilisateur, non fournie à Claude).
Claude Code co-ingénieur (2026-10-09) : AutomationServer + pont MCP `tsaraloha-mcp.exe` (docs/MCP.md), test 200,
vérifié en conditions réelles sur TSALab ; serveurs « tsa » et « tsalab » enregistrés dans Claude Code (portée utilisateur).
MISSING : What-If, rapport IA. Voir docs/AI_COENGINEERING.md.

## Viewport
Status: IMPLEMENTED — viewport unique `OccView` (multi-port supprimé), mises à jour incrémentales,
nœuds et fondations affichés depuis le 2026-10-03. Ouverture 4 896 barres ≈ 16 s (Debug).

## Model
Status: IMPLEMENTED — nœuds, poutres, poteaux, dalles, voiles, fondations, treillis, câbles,
sections, matériaux, appuis 6 DDL, révision + observateurs.

## Selection
Status: IMPLEMENTED — clic, Ctrl+clic, fenêtre/capture, tout (Ctrl+A), inverser (Ctrl+Alt+I),
par type, même section, même matériau, niveau actif, plan de travail actif.
PARTIAL — sélection par propriété arbitraire / par proximité : MISSING.

## Properties
Status: IMPLEMENTED — une vue par type, validation, coalescence Undo ; édition groupée (2026-10-07,
`MultiEditSession`, test 191) : en sélection multiple, les champs modifiés de l'élément principal sont reportés aux
éléments du même type (câbles exclus). Panneau non vérifié en GUI.

## Isolation 3D
Status: IMPLEMENTED (code, 2026-10-07) — menu Affichage ▸ Isolation 3D (isoler sélection / type / plan de travail,
masquer, inverser, précédente, tout afficher) dans `OccView::updateElementIsolation`. Non vérifié en GUI.

## Repères de grille
Status: IMPLEMENTED (2026-10-09) — `src/Grid/GridLabelLayout` : une occurrence par axe selon la vue (plan / élévation ∥ X ou Y / 3D), option deux extrémités. Non vérifié : grilles cylindriques en GUI, ViewCube animé.

## WorkPlanes
Status: IMPLEMENTED — plans X/Y/Z détectés depuis le modèle (`CoordinateSystem::detectStructuralPlanes`),
tolérance centralisée `GeometryTolerance::planeMembership`, isolation « Isoler le plan ».

## 2D Mode
Status: IMPLEMENTED — caméra orthographique normale au plan, isolation recalculée depuis les
drapeaux (plus de fantômes), snap projeté. Vérifié par le code ; pas de test GUI automatisé.

## Modeling tools (outils de modification / dessin)
Status: IMPLEMENTED (2026-10-05) — cadre `TSA::Interaction::ModelingTool` : 14 outils de modification (déplacer,
copier, rotation, symétrie, échelle, réseaux linéaire/polaire, décaler, diviser en N / au point, intersecter,
prolonger, ajuster, fusionner) et 6 de dessin (chaîne, rectangle, portique, contreventement X, arc, poteaux sur
grille). Saisie dans la vue 3D par défaut (clics, valeur clavier, aperçu), fenêtre générique en option
(bouton « Saisie dans la vue 3D », Maj + clic). Tests 140–149 ; vérifié dans l'application : panneaux du ruban,
outil Rectangle par deux clics réels. Non vérifié en GUI : les 19 autres outils (testés au niveau modèle).
Voir docs/MODELING_TOOLS.md.

## Model cleanup (nettoyage topologique)
Status: IMPLEMENTED (2026-10-05) — `TSA::Model::ModelCleanup` : fusion des nœuds confondus, barres en double (charges
reportées), raccordement des nœuds posés sur une barre (division), croisements (option, désactivée par défaut),
nœuds parasites. Bilan sans modification (analyze sur copie), application en une transaction. Fenêtre « Nettoyer le
modèle » (Modifier › Symétrie & Topologie, menu Édition) ; proposé automatiquement avant chaque calcul (F5 / fenêtre
Analysis) si le bilan n'est pas vide. Vérifié dans l'application : fenêtre et bilan. Tests 160–165.

## Editing
Status: PARTIAL — propriétés, déplacement/rotation/copie, suppression, presse-papier, transactions,
symétrie (copie / retournement, plans X/Y/Z), division de poutres/poteaux en N tronçons, fusion des
nœuds confondus (Model_Topology.cpp, tests 101–104 ; UI vérifiée par compilation seulement).
MISSING : division d'une barre à un nœud existant / intersection de barres, symétrie par plan
quelconque, ouvertures de dalles/voiles, poignées (grips) dans le viewport.

## Undo/Redo
Status: IMPLEMENTED — snapshots + transactions + historique structuré + budget mémoire ; niveaux, axes et grilles
dans l'instantané depuis le 2026-10-07 (test 188). Plans de travail : état d'affichage, non annulés (choix assumé).

## Loads
Status: IMPLEMENTED — cas, combinaisons, charges nodales et sur barres (poutre/poteau/treillis/câble),
persistés dans le chunk LOAD (.tsa 1.1). Charges surfaciques / thermiques : MISSING.

## Structural Calculation
Status: PARTIAL — **analyse linéaire seule** : statique linéaire et modale (ADR-025 ; pushover / temporel retirés par
ADR-022, archive `archive/dynamique` ; l'analyse modale est présente dans le code).
OpenSees 3.8.0 externe (Tcl, QProcess) pour barres, treillis, câbles, appuis/ressorts ; rotules `-releasey/-releasez`
(BUG-027) ; charges trapézoïdales par forces d'encastrement parfait ; nœuds de treillis purs stabilisés (BUG-019).
Calcul dans un thread de travail avec progression annulable (BUG-001, à vérifier en GUI). Dalles et voiles non
transmis (BUG-002, avertissement avant calcul).

## Analysis engines (multi-moteurs)
Status: IMPLEMENTED (2026-10-05) — `AnalysisEngine` / registre / `AnalysisManager` ; portées : modèle complet,
sélection, axe de grille, niveau, plan de travail ; OpenSees, Custom2D (MetDeDeplacement 2), Fem3D. Types : statique
linéaire et modale (non linéaire retiré le 2026-10-31, ADR-025). Contexte persisté dans le .tsa (chunk SETT, 2026-10-07,
test 189) ; type ou réglage retiré : conservé, signalé, calcul refusé (tests 532-536).

## Results
Status: IMPLEMENTED — déformée continue sur les stations du solveur (repli Hermite), structure initiale atténuée / masquée, échelle manuelle conservée (2026-10-09, BUG-045 à 048) ; ResultsModel en convention RDM (N > 0 traction, M > 0 fibre négative tendue, V = dM/dx),
stations exactes par équilibre du tronçon, déformée par double intégration de la courbure (tests 181–184) ; équilibre
contrôlé en forces ET moments (BUG-017) ; déformée / diagrammes / réactions 3D, diagrammes 2D, NDC (liens avec famille,
BUG-018), dock « Données d'analyse », export, invalidation automatique. Résultats non enregistrés dans le .tsa (BUG-013).
Courbes 2D (dock « DIAGRAMMES 2D », 2026-10-30, branche feature/diagram-multi-curves, NON commitée) : plusieurs grandeurs
superposées (N/Vx, Vy, Vz, Mx, My, Mz, ux, uy, uz, |u|, θx, θy, θz ; groupes prédéfinis) ou empilées ; un axe par unité
(2 au plus, zéros alignés) sinon normalisée ; extrêmes à leur position ; curseur multi-valeurs ; zoom X/Y ; export image /
valeurs ; barres de treillis et câbles ; suit la sélection 3D ; réglages QSettings « Diagram2D ». Vérifié : test 531 (rendu
contrôlé visuellement sur images), BUG-145. Non vérifié : essai dans TSA.exe réel (sélection 3D → dock, puces, export).

## Meshing
Status: MISSING — aucun mailleur EF pour dalles/voiles.

## .tsa
Status: IMPLEMENTED — chemins non ASCII, JSON COOR/GRID > 64 Kio, nom/auteur et ouverture atomique corrigés le 2026-10-09 (BUG-040 à 043) ; format 1.4 (PROJ, THMB, COOR, GRID, NODE, SUPP, BARS, COLS, SLAB, WALL, FNDN, TRUS, CABL,
LOAD, SNAP, BIMM, SETT), zlib, CRC32, écriture atomique, lecture des formats antérieurs. RSLT non écrit.

## Import/Export
Status: PARTIAL — export NDC PDF/HTML, export rapport de diagnostic ; import/export CAO
(IFC/STEP/DXF) : MISSING (aucune référence dans src/).

## Performance
Status: PARTIAL — mesures de référence : voir changelog 2026-10-03 et test 100.

## Threading
Status: PARTIAL — calcul (AnalysisManager) dans un `QThread::create` lancé par `MainWindow::runAnalysis`, rappels
renvoyés au thread UI ; reste de l'application mono-thread.

## Couche BIM (ADR-020)
Status: PARTIAL — phases 1–3, 8, 9 et 4 (sauf repère de grille) de docs/BIM_ARCHITECTURE.md (2026-10-06).
IMPLEMENTED : `src/BIM/Core` (BimTypes, IfcGuid, BimModel), `src/Core/Units.h` ; `Model::bim()` (sync paresseuse),
`bimForEdit()`, `setBim()` ; snapshot Undo ; hooks division / copie ; chunk `.tsa` BIMM (format 1.3).
IFC (`src/BIM/IFC`) : StepWriter/StepReader, IfcMapper, IfcGeometryMapper, IfcPropertyMapper, IfcRelationshipMapper,
IfcExporter (IFC4X3_ADD2, physique + analytique), IfcImporter (IFC2X3/4/4X3, analytique ou déduit de la géométrie,
unités du fichier). UI : Fichier > Importer / Exporter IFC, ruban Accueil > Projet (MainWindow_Bim.cpp).
Preuves : tests 170–180 ; IfcOpenShell 0.9 validate = 0 anomalie, 13/13 géométries ; essai GUI 2026-10-06 (import du
fichier IfcOpenShell puis export, fichier exporté valide).
MISSING : validation BIM pré-calcul, IDS, BCF, API IA, propriétés BIM dans le panneau Propriétés, AnalysisRun /
historique des versions, charges IFC (BUG-028). Collage : métadonnées transmises (BUG-029, test 190).
