# Architecture Decisions

Last Updated: 2026-10-03. Décisions constatées dans le code ou prises lors des sessions de 2026-10-03.

## ADR-001
Title: Viewport unique
Decision: Un seul viewport 3D `OccView` (V3d_View + AIS_InteractiveContext), encapsulé par
`TSA::UI::ViewportContainer` (règles, sélecteur de plan X/Y/Z, bouton 2D). Le système multi-port
(src/UI/Port) a été supprimé (commit 3276633).
Reason: simplicité, un seul contexte AIS, synchronisation sélection/caméra triviale.
Status: ACTIVE

## ADR-002
Title: Modèle structural = source de vérité, synchronisation par observateurs
Decision: `TSA::Model::Model` possède toutes les données ; les vues dérivent du modèle via
`IModelObserver` (add/modify/remove par type, `onModelDiffApplied`, `onModelCleared`,
`onModelEdited`, `onModelDestroyed`). Aucune donnée métier n'est lue depuis OCCT ou un widget.
Reason: cohérence UI ↔ modèle ↔ 3D ↔ calcul (AGENTS.md §10).
Status: ACTIVE

## ADR-003
Title: Undo/Redo par snapshots complets + transactions
Decision: `UndoManager` stocke des `ModelStateSnapshot` complets ; l'annulation applique un
`ModelDiff` ciblé. Les opérations composées passent par `EditTransaction` (une entrée, rollback).
Pas de migration vers des deltas.
Reason: mesuré (test 100, 4 896 barres, Debug) : snapshot 10 ms, Undo 54 ms → le temps ne justifie
pas une refonte ; la mémoire (~3,2 Mio/snapshot) est bornée par un budget de 512 Mio.
Status: ACTIVE (à réévaluer si les modèles dépassent ~50 000 éléments)

## ADR-004
Title: Historique structuré (EditRecord) pour le co-engineering
Decision: chaque entrée d'historique peut porter des `EditRecord` (action, objet, propriété,
avant, après, impacts) ; `UndoManager::undoHistory()` les expose.
Reason: préparer un historique lisible et un assistant IA qui comprend QUI/QUOI/AVANT/APRÈS/IMPACT.
Status: ACTIVE (alimentation partielle, voir BUG-006)

## ADR-005
Title: Format .tsa binaire à chunks, compatibilité ascendante
Decision: en-tête fixe 256 o (magic `TSAF`), chunks FourCC avec taille (chunks inconnus ignorés),
zlib, CRC32 ; version mineure incrémentée pour tout ajout non cassant (1.1 = chunk LOAD) ; écriture
atomique via QSaveFile. Spécification : docs/TSA_FILE_FORMAT.md.
Status: ACTIVE

## ADR-006
Title: Invalidation des résultats par révision du modèle
Decision: `Model::revision()` (incrémentée par toute notification et `setModified(true)`) ;
`ResultsValidityGuard` invalide les résultats au premier écart avec la révision analysée.
Un Undo ne revalide jamais des résultats.
Reason: ne jamais présenter des résultats d'un modèle différent comme valides.
Status: ACTIVE

## ADR-007
Title: Sélection unique, requêtes ensemblistes dans le cœur
Decision: `SelectionManager` est l'unique système de sélection ; les sélections avancées sont des
fonctions pures `TSA::Model::SelectionQuery` produisant un `ElementSet` appliqué par
`SelectionManager::selectElements` (un seul signal).
Status: ACTIVE

## ADR-008
Title: Tolérance géométrique centralisée
Decision: `TSA::Coordinate::GeometryTolerance::planeMembership` (0,05 m) pour la détection des plans,
l'isolation 2D et la sélection « sur le plan » ; `pointCoincidence` (1e-6 m).
Status: ACTIVE

## ADR-009
Title: Niveaux — seuls les nœuds rattachés suivent un changement d'élévation
Decision: `Model::onLevelElevationChanged` ne déplace que les nœuds dont `levelId` = niveau
(attribué par `Model::addNode` à la cote d'un niveau existant) ; notification groupée.
Reason: aucun déplacement silencieux (choix validé par l'utilisateur, option a).
Status: ACTIVE

## ADR-010
Title: Calcul par OpenSees externe
Decision: le calcul passe par un snapshot immuable (`CalculationSnapshot`) traduit en script Tcl
exécuté par OpenSees via `QProcess` ; seul src/Analysis dépend d'OpenSees.
Status: ACTIVE (exécution synchrone côté UI : BUG-001)

## ADR-011
Title: Threading
Decision: tout accès Qt Widgets et OCCT (AIS/V3d) reste sur le thread UI ; les mises à jour d'UI
déclenchées au milieu d'une notification modèle sont différées (`QTimer::singleShot(0)`).
Status: ACTIVE

## ADR-013
Title: Correspondance TSA ↔ OpenSees centralisée
Decision: `OpenSeesModelMap` (construit depuis le snapshot) est l'unique source des tags OpenSees, axes locaux,
nœuds auxiliaires de ressorts et dispositions des recorders ; générateur et lecteur l'utilisent tous deux.
Éléments désignés par `ElementKey {famille, id}` partout dans les résultats ; tag OpenSees unique 1..N.
Reason: les ids TSA ne sont uniques que par famille (collision poutre/poteau constatée), et le lecteur devinait
la disposition des fichiers.
Status: ACTIVE (2026-10-04)

## ADR-014
Title: Résultats avancés optionnels par passage OpenSees séparé
Decision: `ExtractionLevel::Light` (défaut) / `Advanced`. En Advanced, un second script sans charge
(`initialize`, `record`, `nodeDOFs`, `printA -ret` en `system FullGeneral`) fournit mapping DDL, k_basic et
K_global ; le solveur de l'analyse principale n'est jamais modifié. K_global plafonnée (1 500 DDL par défaut),
stockée en COO creux ; chaque matrice porte un `MatrixMetadata` (source API/reconstruite, type, repère, exact).
Reason: OpenSees 3.8.0 n'expose la matrice du système que pour FullGeneral (dense) ; changer le solveur pour
l'obtenir modifierait le calcul.
Status: ACTIVE (2026-10-04) — voir docs/OPENSEES_RESULTS.md

## ADR-012
Title: Performance — mises à jour locales
Decision: modification d'un élément → `update*Shape` de cet élément (et éléments connectés pour un
nœud), sans `rebuildAllShapes` ; reconstruction complète réservée à l'ouverture / reset, en une
passe et un redraw ; génération NDC différée tant que le dock n'est pas visible.
Status: ACTIVE
