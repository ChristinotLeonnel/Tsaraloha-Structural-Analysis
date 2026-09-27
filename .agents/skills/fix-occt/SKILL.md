---
name: fix-occt
description: Diagnostiquer et corriger un bug de géométrie 3D/OCCT (forme incorrecte, sélection, viewer) en remontant du modèle jusqu'à l'affichage AIS/OccView.
---

# fix-occt

Objectif : corriger un bug 3D/OCCT en identifiant la couche réelle en cause, du modèle
jusqu'au viewer, sans redessiner toute la scène par réflexe.

## Chaîne de diagnostic

```text
Model
→ Parameters
→ Geometry Builder
→ OCCT Shape
→ AIS
→ Viewer (OccView)
```

1. **Model** — les paramètres source (`Section`, `Material`, `Node`s/coordonnées) sont-ils
   corrects dans le modèle ?
2. **Parameters** — ces paramètres sont-ils correctement transmis au Geometry Builder
   concerné (`src/Geometry/*Geometry`) ?
3. **Geometry Builder** — la construction géométrique (`TopoDS_Shape`) est-elle correcte
   pour ces paramètres ? Isoler ce point avec un test unitaire si possible plutôt que de
   déboguer uniquement visuellement.
4. **OCCT Shape** — la `Shape` produite est-elle valide (pas de shape dégénérée, bonne
   orientation) ?
5. **AIS** — l'objet `AIS_Shape` correspondant est-il bien mis à jour (pas une ancienne
   instance orpheline) ?
6. **Viewer (OccView)** — l'affichage (redraw, sélection, `SelectionManager`) reflète-t-il
   bien l'`AIS_Shape` à jour ?

## Éviter les redraw complets inutiles

- Corriger la mise à jour ciblée de l'élément concerné (via son callback
  `IModelObserver`) plutôt que de forcer un rebuild complet de la scène comme solution de
  facilité — un rebuild complet masque souvent la vraie cause sans la corriger.

## Cas fréquents à vérifier

- **Copie** : la géométrie d'un élément copié doit être reconstruite depuis le modèle
  copié, pas partagée avec la `Shape` source (cause fréquente d'artefacts après
  Copy/Paste).
- **Section/Matériau** : une divergence visuelle après changement de section/matériau
  vient presque toujours d'un Geometry Builder qui n'a pas été notifié
  (`onXModified` manquant ou mal câblé), pas d'un bug OCCT en tant que tel.
- **Plans de coupe** (`Graphic3d_ClipPlane`) : vérifier qu'ils sont bien de simples outils
  de visualisation et ne modifient jamais le modèle.

## Après correction

- Vérifier la performance (pas de reconstruction globale introduite par la correction).
- Ajouter/adapter un test si le Geometry Builder concerné est testable indépendamment du
  rendu OpenGL.
