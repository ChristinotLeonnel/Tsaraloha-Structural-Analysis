# Système d'édition — TSA

| Métadonnée | Valeur |
| :--- | :--- |
| Document | Spécification technique du système d'édition (Undo/Redo, transactions, sélection) |
| Version du document | 1.0 |
| Version logicielle | TSA 0.1.0 — branche `feature/edit-system` |
| Date | 2026-10-03 |
| Public | Développeurs TSA et agents IA (voir `AGENTS.md`) |
| Prérequis | `docs/ARCHITECTURE.md`, `docs/MODEL.md` |

> Document vivant : en cas de divergence, le code fait foi.

## 1. Principe

Toute modification du modèle suit le même chemin, sans système parallèle :

```text
Action utilisateur (Propriétés, viewport, menu, console)
   ↓
Validation (bornes des champs, Model::wouldCollapseConnectedElement, ...)
   ↓
Entrée d'historique : Model::pushUndoState / EditTransaction / CommandManager
   ↓
Modification du modèle (source de vérité)
   ↓
Notification IModelObserver (notify*Modified, ModelDiff)  →  révision du modèle +1
   ↓
Vues : OccView (géométrie locale), ModelTreeWidget, PropertyPanel
   ↓
ResultsValidityGuard : résultats de calcul marqués obsolètes
```

## 2. Historique Undo/Redo (`src/UndoRedo`)

| Élément | Rôle |
| :--- | :--- |
| `UndoManager` | Pile de snapshots complets du modèle (`Model::ModelStateSnapshot`). L'annulation applique un `ModelDiff` : seules les formes modifiées sont reconstruites. |
| `Model::pushUndoState(nom, cléDeCoalescence)` | Une entrée par action. Avec une clé, des appels successifs sur la même clé à moins de 2 s fusionnent (crans de spinbox). |
| `EditTransaction` (RAII) | Opération composée = **une** entrée. `pushUndoState` imbriqués absorbés. `rollback()` (ou destruction sans `commit()`) restaure le modèle et notifie les vues. |
| `CommandManager::executeCommand` | Exécute une `ICommand` dans une transaction : aucune entrée si la commande échoue, modification partielle annulée, exceptions interceptées. |
| `EditRecord` / `HistoryItem` | Enregistrement structuré attaché à chaque entrée : action, objet, propriété, avant, après, impacts. `UndoManager::undoHistory()` l'expose (historique lisible, co-engineering). |

Limites mesurées (Debug, 4 896 barres, test 100) : snapshot 10 ms, Undo 54 ms, ~3,2 Mio par
snapshot. L'historique est borné à 50 niveaux **et** à un budget mémoire de 512 Mio
(`UndoManager::setMemoryBudgetBytes`). Les snapshots ne couvrent pas les niveaux, grilles et
plans de travail (voir § 6).

### Exemple

```cpp
TSA::UndoRedo::EditTransaction tx(model, "Rotation de 20 poteaux");
for (int id : ids)
{
    model.getColumn(id)->setRotation(90.0);
    model.notifyColumnModified(id);
    tx.record({ "modify_property", "Column", id, "rotation", "0°", "90°", { "geometry", "stiffness" } });
}
tx.commit(); // une seule entrée « Rotation de 20 poteaux »
```

## 3. Panneau Propriétés

- Les champs numériques n'appliquent leur valeur qu'à Entrée / perte du focus / cran de flèche
  (`keyboardTracking` désactivé centralement dans `PropertyPanel`).
- Chaque vue utilise `pushUndoState(nom, nom)` : les crans successifs sur le même objet forment
  une seule entrée.
- Nœud : coordonnées refusées si un élément connecté deviendrait de longueur nulle.
- Poutre / poteau : changement de section ou de matériau enregistré dans l'historique.

## 4. Résultats de calcul

`Model::revision()` augmente à chaque notification et à chaque `setModified(true)`.
`ResultsValidityGuard` (src/Analysis) mémorise la révision analysée ; au premier écart,
`ResultsModel::invalidate()` est appelé et l'interface retire déformée et diagrammes, et la note
de calcul n'affiche plus de résultats. Un Undo ne revalide jamais des résultats : il faut relancer
le calcul.

## 5. Sélection

`SelectionManager` reste l'unique système de sélection. Les requêtes ensemblistes
(`TSA::Model::SelectionQuery`, testables sans interface) produisent un `ElementSet` appliqué en
une fois par `SelectionManager::selectElements()` :

| Commande (`CommandCatalog`) | Requête | Raccourci |
| :--- | :--- | :--- |
| `cmd.select.all` | `all` | Ctrl+A |
| `cmd.select.invert` | `invert` | Ctrl+Alt+I |
| `cmd.select.by_type` | `byKind` | — |
| `cmd.select.same_section` | `sameSection` | — |
| `cmd.select.same_material` | `sameMaterial` | — |
| `cmd.select.active_level` | `atElevation` | — |
| `cmd.select.active_workplane` | `onWorkPlane` (éléments entièrement dans le plan) | — |

Tolérance d'appartenance à un plan : `TSA::Coordinate::GeometryTolerance::planeMembership`
(0,05 m), partagée par la détection des plans X/Y/Z, l'isolation 2D et la sélection.

## 6. Niveaux

Modifier l'élévation d'un niveau déplace uniquement les nœuds **rattachés** à ce niveau
(`Node::levelId`, attribué automatiquement par `Model::addNode` à la cote d'un niveau existant).
Les éléments connectés sont mis à jour par une seule notification groupée. Limite connue : les
niveaux ne font pas partie des snapshots Undo ; annuler une modification d'élévation de niveau
n'est pas encore pris en charge.

## 7. Tests

| Test | Couverture |
| :--- | :--- |
| 95 | Révision du modèle, invalidation des résultats (y compris après Undo) |
| 96 | Transactions, rollback, imbrication, commande en échec, historique structuré |
| 97 | Coalescence des entrées, validation géométrique des nœuds |
| 98 | Requêtes de sélection |
| 99 | Élévation de niveau (nœuds rattachés uniquement) |
| 100 | Mesures sur 4 896 barres, budget mémoire de l'historique |
