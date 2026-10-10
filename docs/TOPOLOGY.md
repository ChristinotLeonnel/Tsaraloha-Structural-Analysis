# Topologie et numérotation — TSA

Accès : **Fichier > Paramètres du projet > Topologie et numérotation**, ou ruban **Outils > Projet >
Topologie et numérotation**. Aide en ligne : identifiant `project.topology` (page `docs/modeling/numbering`).

## Principes

| Notion | Où | Modifiée par une renumérotation |
| :--- | :--- | :--- |
| Identifiant interne | `Node::id`, `Beam::id`… (clé des `std::map` du `Model`) | jamais |
| Étiquette visible | `Node::name` / `formattedName()`, idem éléments | oui |
| Numéro solveur | OpenSees : tag nœud = identifiant interne ; tag élément = `CalculationSnapshot` (ordre Beam, Column, Truss, Cable) | jamais |

Une renumérotation ne touche ni coordonnées, ni connectivités, ni orientation des barres, ni sections,
matériaux, charges, appuis. Elle passe par `Model::pushLabelUndoState` (une entrée Annuler, révision
inchangée) et `Model::notifyLabelsChanged` (vues notifiées, document modifié, **résultats conservés**).

## Architecture (`src/Topology`, couche modèle)

| Fichier | Rôle |
| :--- | :--- |
| `TopologySettings.*` | Paramètres persistants (stratégies, ordre des axes, tolérances, formats, préfixes, portée, affichage) ; JSON versionné `schema` |
| `NumberingStrategies.*` | Interfaces `NodeNumberingStrategy` / `ElementNumberingStrategy`, registres, algorithmes (tri par axes avec regroupement par tolérance, grille, niveaux, BFS, DFS, Cuthill-McKee inverse) |
| `TopologyNumberingService.*` | `validateSettings`, `computePreview` (sans effet), `applyPreview` (atomique, refuse un aperçu périmé), `solverMapping` |

Interface : `src/UI/Dialogs/TopologyDialog.*` (onglets Général, Nœuds, Éléments, Maillage, Aperçu,
Correspondances solveur) ; `src/UI/MainWindow_Topology.cpp` (grille active, niveaux, sélection, affichage).

Une stratégie produit seulement un **ordre** (et facultativement repère de grille `{g}`, couche `{l}`) ; la
génération des étiquettes (départ, pas, chiffres, format, conservation des étiquettes personnalisées,
redémarrage par couche) et la détection des doublons sont communes.

### Ajouter une stratégie

1. Une classe dérivée de `NodeNumberingStrategy` ou `ElementNumberingStrategy` : `id`, `name`,
   `description`, `availability` (raison si indisponible), `order`.
2. Une ligne dans `nodeStrategies()` / `elementStrategies()`.
3. Un test dans `tests/test_topology.cpp`.

La fenêtre (liste, infobulles, désactivation) et les solveurs n'ont rien à modifier.

## Stratégies

| Nœuds | État |
| :--- | :--- |
| Séquentielle, par coordonnées (permutations, sens par axe), X-Y-Z, Z-Y-X, rangées, colonnes, couches (3D), grille d'axes (A1, couche, nœuds confondus « .2 », hors grille sans faux repère), par niveau, Cuthill-McKee inverse, BFS, DFS, personnalisée | implémentées |
| Par sous-modèle ou groupe | indisponible : pas de groupes dans le `Model` |
| Orientées maillage structuré / non structuré | indisponibles : pas de mailleur (BUG-002) |

| Éléments | État |
| :--- | :--- |
| Séquentielle, par type, par proximité (centres), topologique (suit les nœuds), personnalisée ; suites par famille ou commune ; barres de rôle poteau → préfixe des poteaux | implémentées |
| Par groupe ; par maillage structuré / non structuré | indisponibles (mêmes raisons) |

Non implémenté : mode automatique (étiquette au format choisi dès la création), aperçu des nouvelles
étiquettes dans la vue 3D avant application.

## Persistance

Chunk `TOPO` du `.tsa` (format 1.5, JSON UTF-8, `TopologySettings::toJson`). Absent (projets ≤ 1.4) :
valeurs par défaut reproduisant les étiquettes historiques (N001, B001, C001, TR001, K001, S001, W001,
F001), aucune intervention nécessaire. Lecture tolérante : champ invalide → défaut, signalé.

```json
{"schema":1,"scope":"project","showNodeLabels":true,
 "nodes":{"strategy":"grid","order":{"axes":"ZYX","descX":false,"descY":false,"descZ":false},
          "tolerance":0.001,"startNodeId":0,"restartPerLayer":false,"preserveCustomNames":true,
          "format":{"prefix":"N","pattern":"{g}","width":3,"start":1,"increment":1}},
 "elements":{"strategy":"sequential","sharedSequence":false,"order":{...},"tolerance":0.001,
             "preserveCustomNames":true,"format":{...},"prefixes":{"beam":"B","column":"C",...}}}
```

## Futurs mailleurs et solveurs

- **Mailleur** : exposer les nœuds et éléments de maillage comme familles supplémentaires
  (`EntityFamily`), puis implémenter les stratégies « orientées maillage » (ordre structuré i, j, k ; ordre
  non structuré par Cuthill-McKee sur le graphe du maillage) en remplaçant `UnavailableNodes`. Ne jamais
  réutiliser les étiquettes comme clés : garder un identifiant interne stable par entité de maillage.
- **Solveur** : lire les identifiants internes (`AnalysisMapping`, `CalculationSnapshot`) et produire sa
  propre numérotation compacte si nécessaire ; la publier dans `solverMapping` pour l'onglet Correspondances.

## Tests

`tests/test_topology.cpp` (suite `topology`, tests 225-236) : paramètres et JSON, tolérances, toutes les
stratégies, 1D/2D/3D, modèle vide, composantes multiples, départ/pas/préfixe/format, doublons,
conservation, portée sélection, non-régression (coordonnées, connectivités, propriétés, charges, appuis),
révision inchangée, Annuler, aperçu périmé, chunk TOPO, anciens fichiers, correspondance solveur, fenêtre Qt.
