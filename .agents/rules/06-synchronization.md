---
title: Synchronisation UI / Model / Geometry / 3D
scope: repo
applies_to: ["src/Model/**", "src/Geometry/**", "src/Viewer/**", "src/UI/**", "src/Commands/**", "src/UndoRedo/**"]
---

# 06 — Synchronisation

## Règle générale

```text
UI → Model → Geometry → 3D
```

et dans l'autre sens :

```text
Sélection 3D → Model → UI
```

Le mécanisme réel qui implémente le sens descendant (Model → 3D/UI) est
`TSA::Model::IModelObserver`, une interface de callbacks granulaire par type d'élément
(`onNodeAdded/Modified/Removed`, `onBeamAdded/Modified/Removed`,
`onColumnAdded/Modified/Removed`, `onSlabAdded/Modified/Removed`,
`onWallAdded/Modified/Removed`, `onFoundationAdded/Modified/Removed`,
`onTrussMemberAdded/Modified/Removed`, `onCableAdded/...`, etc.). `OccView` implémente
cette interface directement.

`TODO: VERIFY IN SOURCE` : confirmer si l'UI (`ModelTreeWidget`, `PropertyPanel`)
implémente aussi `IModelObserver` directement ou passe par un relais Qt (signal émis par
un objet qui, lui, implémente `IModelObserver`).

## Cas à couvrir pour toute nouvelle fonctionnalité touchant un élément

1. **Création** — passe par une `ICommand` (ex. `CreateBeamCommand`), qui modifie le
   `Model`, qui notifie via `onXAdded`, qui déclenche la génération de géométrie
   (`*Geometry` builder) et la mise à jour de l'UI.
2. **Modification** — la commande (ou modification directe suivie d'un `pushState`) émet
   `onXModified` ; la géométrie concernée est reconstruite (pas toute la scène) ; l'UI
   (propriétés + arbre) se rafraîchit depuis le modèle.
3. **Copie** — via `StructuralClipboard` : le nouvel élément est un objet modèle distinct
   avec une géométrie **recréée**, jamais une référence partagée à la `Shape` OCCT source.
4. **Déplacement** — traité comme une modification des coordonnées/`Node`s référencés ;
   déclenche `onNodeModified` et/ou `onXModified` selon ce qui est réellement stocké.
5. **Suppression** — `onXRemoved`, doit nettoyer la représentation OCCT associée dans
   `OccView` (pas de `Shape` orpheline).
6. **Undo** — `UndoManager::undo` restaure un `ModelStateSnapshot` complet ; les vues
   (UI + 3D) doivent se resynchroniser entièrement depuis cet état restauré, pas
   partiellement.
7. **Redo** — symétrique à Undo.

## Une seule source de vérité

Le `Model` est la source de vérité unique. Si une divergence apparaît entre UI, modèle et
3D, identifier la **première couche** où la divergence apparaît (voir skill
`verify-sync`) plutôt que de corriger le symptôme dans la couche où il est visible.

## Vérification

`TODO: VERIFY IN SOURCE` pour la chaîne exacte de propagation entre `IModelObserver` et les
signals Qt de l'UI — lire `src/UI/ModelTree/ModelTreeWidget.cpp` et
`src/UI/Properties/PropertyPanel.cpp`.
