---
name: verify-sync
description: Vérifier la cohérence entre la valeur UI, la valeur du modèle, la valeur de la géométrie et la représentation 3D pour une propriété donnée, et localiser la première couche où une divergence apparaît.
---

# verify-sync

Objectif : quand un comportement semble incohérent entre ce que voit l'utilisateur et ce
que contient réellement le modèle, localiser précisément la première couche où la
divergence apparaît plutôt que de deviner.

## Vérification

```text
UI value
=
Model value
=
Geometry value
=
3D representation
```

Pour la propriété concernée (ex. une dimension de section, un matériau, une position de
nœud) :

1. **UI value** — lire la valeur telle qu'affichée dans `PropertyPanel`/`ModelTreeWidget`.
2. **Model value** — lire la valeur réelle stockée dans l'objet modèle correspondant
   (`Section`, `Material`, `Node`, ou propriété de l'`Element` concerné) — au débogueur ou
   via un point de log temporaire, pas seulement par lecture de code.
3. **Geometry value** — vérifier les paramètres réellement transmis au Geometry Builder
   (`src/Geometry/*Geometry`) au moment de la construction de la `Shape`.
4. **3D representation** — vérifier ce que l'`AIS_Shape` affichée dans `OccView`
   représente effectivement (dimensions, position).

## Identifier la première couche où apparaît la divergence

- Si UI ≠ Model : le bug est dans la commande d'écriture ou dans le rafraîchissement UI
  (voir skill `fix-ui`).
- Si Model = UI mais Geometry ≠ Model : le bug est dans la transmission des paramètres au
  Geometry Builder ou dans le Geometry Builder lui-même (voir skill `fix-occt`).
- Si Geometry = Model mais 3D ≠ Geometry : le bug est dans la mise à jour de l'`AIS_Shape`
  ou dans `OccView` (rafraîchissement manquant, `Shape` orpheline) — voir skill
  `fix-occt`.

## Corriger à la source

Toujours corriger à la couche où la divergence apparaît réellement, jamais en ajoutant une
correction cosmétique à une couche en aval (ex. ne jamais « forcer » l'affichage 3D à
matcher l'UI si c'est en réalité le modèle qui est incorrect).

## Après correction

Refaire la vérification complète des quatre couches pour confirmer la cohérence
retrouvée, et ajouter un test couvrant ce scénario si la synchronisation d'un nouveau type
de propriété n'était pas encore testée.
