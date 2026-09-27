---
title: Architecture
scope: repo
applies_to: ["src/**", "Extensions/**"]
---

# 01 — Architecture

## Séparation en couches

L'architecture réelle constatée dans `src/` sépare :

```text
UI (src/UI) → Commands/UndoRedo (src/Commands, src/UndoRedo) → Model (src/Model)
  → Geometry (src/Geometry) → OCCT/Viewer (src/Viewer)
```

avec des modules transversaux : `src/Coordinate`, `src/Grid`, `src/ExtensionSystem`
(TSALib), `src/IO`, `src/Diagnostics`, `src/Interaction`.

- Ne jamais faire dépendre `src/Model` de `src/UI`, `src/Viewer` ou de tout header Qt/OCCT
  côté interface publique du modèle métier. Le modèle doit rester indépendant de sa
  représentation.
- `src/Geometry` peut dépendre de `src/Model` (lecture) mais pas l'inverse.
- `src/Viewer` peut dépendre de `src/Model` (il observe via `IModelObserver`) et de
  `src/Geometry`, jamais l'inverse.

## Réutilisation avant création

Avant toute nouvelle classe/interface/abstraction :

1. chercher dans `src/` un équivalent existant (`grep`/recherche par nom métier) ;
2. lire l'implémentation existante, pas seulement le header ;
3. évaluer si une extension ou une spécialisation suffit ;
4. créer uniquement si aucune réutilisation n'est raisonnable.

## Éviter les duplications

- Ne pas dupliquer une commande, un `IModelObserver`, un widget de propriété qui existe
  déjà pour un type d'élément voisin — voir la règle « Aucun Doublon de Commande » plus bas
  dans `AGENTS.md` (section historique conservée), qui reste pleinement applicable.
- Un même concept (ex. section, matériau, niveau) ne doit avoir qu'une seule
  implémentation canonique (`src/Model/Section.h`, `src/Model/Material.h`,
  `src/Coordinate/Level.h`).

## Éviter les dépendances circulaires

- Respecter le sens des flèches ci-dessus. Si une couche « basse » (Model, Geometry)
  semble avoir besoin d'une couche « haute » (UI, Viewer), c'est un signal qu'une
  abstraction manque (ex. `IModelObserver` côté Model, implémenté côté Viewer) plutôt
  qu'une raison d'inverser la dépendance.
- Les `namespace` (`TSA::Model`, `TSA::Geometry` implicite par dossier, `TSA::Viewer`,
  `TSA::Coordinate`, `TSA::Commands`, `TSA::UndoRedo`) doivent refléter cette séparation :
  ne pas introduire d'inclusion croisée qui la contredirait.

## Vérification

`TODO: VERIFY IN SOURCE` — confirmer, avant modification structurelle importante, que cette
description correspond toujours au CMakeLists.txt (cibles, dépendances) et à l'arborescence
réelle de `src/`.
