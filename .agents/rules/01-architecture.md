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

## Règle de responsabilité unique (SRP)

Une classe ou un module doit avoir une **responsabilité principale clairement identifiable**.

- Séparer strictement :
  - **Données métier** (`src/Model` : `Beam`, `Column`, `Slab`, `Cable`, `Section`, `Material`)
  - **Géométrie 3D B-Rep** (`src/Geometry` : `BeamGeometry`, `SlabGeometry`, `CableGeometry3D`)
  - **Visualisation & Interaction 3D** (`src/Viewer` : `OccView`, `SelectionManager`)
  - **Interface utilisateur** (`src/UI` : `Ribbon`, `PropertyPanel`, `ModelTreeWidget`)
  - **Mutations & Historique** (`src/Commands` : `ICommand`, `src/UndoRedo` : `CommandManager`)
  - **Persistance** (`src/IO` : format `.tsa`)
- Éviter impérativement les classes fourre-tout concentrant UI, données, calcul, affichage et sauvegarde.

## Règle de couplage et flux de dépendances

Lors de tout ajout ou modification, respecter le flux unidirectionnel :

```text
UI
 ↓
Command / Application (ICommand, CommandManager)
 ↓
Structural Model (TSA::Model::Model — Source de vérité unique)
 ↓
Geometry (*Geometry builders)
 ↓
OCCT (AIS_Shape, OccView)
```

Et pour le calcul structural :
```text
Structural Model
 ↓
Analysis Model
 ↓
Solver / OpenSees Adapter
 ↓
Results
```

Interdictions strictes :
- Pas de `UI → OCCT` direct sans passer par le modèle.
- Pas de `UI → IO (.tsa)` direct.
- Pas de `UI → Solver` direct.
- Pas de modification directe des conteneurs internes du `Model` sans passer par les commandes / UndoRedo.

## Échelle de surveillance de la taille du code

La taille du code s'évalue en fonction de la **responsabilité**, de la **complexité**, du **couplage**, de la **testabilité** et de la **lisibilité** :

```text
< 300 lignes
→ taille généralement confortable

300–600 lignes
→ normale

600–1 000 lignes
→ surveiller la responsabilité

> 1 000 lignes
→ analyser systématiquement la possibilité de découpage (signal d'analyse, pas limite absolue)

> 2 000 lignes
→ refactorisation à envisager sérieusement

> 5 000 lignes
→ fichier potentiellement monolithique ; analyse architecturale obligatoire
```

**Règle :** Ne jamais découper artificiellement un fichier pour une simple métrique, et ne jamais compresser le code au détriment de la lisibilité.

## Protocole de refactorisation sécurisée

Avant tout découpage ou réorganisation d'une classe :
1. identifier sa responsabilité ;
2. identifier ses dépendances ;
3. identifier les appels entrants ;
4. identifier les appels sortants ;
5. vérifier les signaux/slots Qt ;
6. vérifier les références OCCT ;
7. vérifier les tests unitaires existants ;
8. vérifier CMake (`CMakeLists.txt`) ;
9. évaluer le risque de régression.

## Hiérarchie absolue des priorités

```text
CORRECTNESS → ARCHITECTURE → MAINTAINABILITY → TESTABILITY → PERFORMANCE → Taille du code
```

## Vérification

`TODO: VERIFY IN SOURCE` — confirmer, avant modification structurelle importante, que cette
description correspond toujours au CMakeLists.txt (cibles, dépendances) et à l'arborescence
réelle de `src/`.
