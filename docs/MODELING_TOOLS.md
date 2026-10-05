# Outils de modification et de dessin

Tous les outils de modification et de dessin rapide partagent un cadre unique
(`src/Interaction/Tools`). Par défaut, ils se pilotent **directement dans la vue 3D** ; une fenêtre de
paramètres reste disponible.

## Modes de saisie

| Mode | Comment | Réglage |
| :--- | :--- | :--- |
| Vue 3D (défaut) | clics accrochés (nœuds, grilles, plan de travail), barre sous le curseur, valeur tapée au clavier + Entrée, aperçu (traits orange + fantômes de la sélection) | bouton « Saisie dans la vue 3D » (onglet Modifier, menu Édition), mémorisé (QSettings `modeling/inputInViewport`) |
| Fenêtre | formulaire généré à partir des paramètres de l'outil (`ModelingToolDialog`), appliqué à la sélection | décocher le bouton, ou **Maj + clic** sur l'outil (inverse le mode une fois) |

Clavier en saisie 3D : chiffres, `.`/`,`, `-` puis **Entrée** (distance dans la direction du curseur,
angle, facteur, nombre selon l'étape) ; **Entrée** seule termine une chaîne ; **Retour arrière** ;
**Échap** efface la valeur tapée, puis recommence l'outil, puis le quitte. **Ctrl + clic** : variante
(rotation → copie, symétrie → retourner sans copier). « Translation / Copie numérique » (Ctrl+Maj+M,
Ctrl+D) ouvrent toujours la fenêtre.

Chaque opération est une transaction : **une** entrée Annuler, rien n'est modifié si l'outil échoue.

## Outils

| Outil | Saisie 3D | Fenêtre |
| :--- | :--- | :--- |
| Déplacer (M) | base → destination ou distance tapée | dx, dy, dz |
| Copier | base → destination (répétable) | dx, dy, dz, nombre |
| Rotation (Ctrl+R) | centre → référence → direction ou angle tapé ; axe = normale du plan de travail | centre, axe, angle, copie, nombre |
| Symétrie | 2 points de l'axe (plan ⟂ plan de travail) | 2 points, normale, copie |
| Échelle | base → facteur tapé, ou base → référence → nouvelle position | base, facteur |
| Réseau linéaire | nombre tapé, base → pas | vecteur, nombre |
| Réseau polaire | nombre tapé, centre | centre, axe, nombre, angle total |
| Décaler | distance tapée, barre → côté | distance, côté |
| Diviser en N | N tapé, clic sur une barre (répétable) | N (sélection) |
| Diviser au point | clic sur la barre (accroché) | position relative (sélection) |
| Intersecter | 2 barres | tolérance (toutes les paires de la sélection) |
| Prolonger | barre limite → extrémité à prolonger | — (vue 3D uniquement) |
| Ajuster | barre de coupe → partie à supprimer | — (vue 3D uniquement) |
| Fusionner les nœuds | immédiat | tolérance |
| Chaîne de poutres | points successifs, Entrée | — (vue 3D uniquement) |
| Rectangle de poutres | 2 coins (axes du plan de travail) | 2 coins |
| Portique | hauteur tapée, 2 pieds | 2 pieds, hauteur |
| Contreventement en X | 4 coins dans l'ordre (2 treillis) | 4 coins |
| Arc de poutres | N tapé, début → passage → fin | 3 points, N |
| Poteaux sur grille | hauteur tapée, 2 coins de zone (grille active) | 2 coins, hauteur |

Les éléments dessinés reprennent les préréglages de création (sections, matériaux, angle β) ; les
nœuds existants sont réutilisés (1 mm). Prolonger, Ajuster, Intersecter créent un nœud commun sur la
barre limite / de coupe (assemblage structurel) et suppriment les nœuds devenus orphelins (sans
élément, appui ni charge). Divisions : mêmes règles de charges que `splitBeam` (refus si charge
ponctuelle ou partielle).

## Architecture

```text
QAction (ruban / menu) → MainWindow::startModelingTool(id)
   ├─ vue 3D : OccView::startModelingTool(tool, ctx) — OccView_Tools.cpp (clics, aperçu, clavier)
   │            └─ signal modelingToolReady → MainWindow::applyActiveModelingTool
   └─ fenêtre : ModelingToolDialog(tool) → applyActiveModelingTool
applyActiveModelingTool : EditTransaction → tool->apply(model, ctx) → commit / rollback → sélection
```

- `ModelingTool` (paramètres + étapes de saisie + `apply`), `ModelingToolRegistry`,
  `registerBuiltInModelingTools` (`DrawTools.cpp`) : ajouter un outil = une classe + une ligne.
- Opérations du modèle utilisées : `moveNodes`, `transformNodes`, `copyElements`,
  `copyAndRotateElements`, `mirrorElements`, `splitBeam/Column`, `splitBarAt`, `mergeCoincidentNodes`.
- Tests : `tests/test_modeling_tools.cpp` (suite `tools`, tests 140-149).
