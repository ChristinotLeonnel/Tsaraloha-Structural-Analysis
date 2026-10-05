# Version d'origine — analyse (2026-10-05)

Code d'origine conservé tel quel (non compilé). Il implémente la méthode des rotations à la main
pour des portiques plans orthogonaux, à partir d'un fichier JSON issu d'un dessin.

## Fonctionnement

- Entrée : `noeuds`, `ln_k` (barres), `rectangle_k` (au milieu d'une barre : section b×h ;
  accolé à une barre : charge répartie dont l'intensité est la hauteur du rectangle × `scale`),
  `circle_k` (au milieu : section circulaire ; autour d'un nœud : rotule).
- Appuis implicites : nœud le plus bas de chaque abscisse x, encastré sauf s'il est dans un cercle.
- Inconnues : rotations des nœuds + translations horizontales d'étage (« appuis fictifs ») pour le vent.
- K = 4EI/L (3EI/L avec rotule), report 0,5, M̄ = qL²/12, consoles traitées isostatiquement.
- Sorties : matrice, second membre, moments Mij, courbes M(x), V(x) échantillonnées (pas 0,01 m)
  et mises à l'échelle du dessin, fichiers JSON.

## Défauts constatés

1. E = 1 et inertie relative : rotations et translations sans unité physique ; aucun déplacement,
   aucune réaction, aucun effort normal.
2. Inconnues `V_nodes` (appuis fictifs verticaux) ajoutées au système sans équation : matrice
   singulière dès qu'il en existe.
3. `SecondMember` : `std::find(Matrix_nodes, "rotule")` cherche un nom de type dans la liste des
   nœuds (toujours faux).
4. Barre avec rotule : raideur 3EI/L mais moment d'encastrement laissé à qL²/12 (il faut qL²/8).
5. Analyse des courbes : division par la charge (q = 0 → NaN).
6. Chemins de sortie codés en dur ; `#pragma` vide.
7. Performances : chaque `Line` / `Node` copie toute la structure de données et reconstruit ses
   voisins (coût quadratique à cubique) ; inversion complète de la matrice (O(n³)).
8. Barres inclinées, charges ponctuelles et charges nodales non traitées.

## Correspondance avec la version 2

| Origine | Version 2 |
| :--- | :--- |
| `ConstanteLine::K` = 4EI/L, report 0,5 | `Element.cpp` : k[2][2] = 4EI/L, k[2][5] = 2EI/L |
| Rotule (cercle) → 3EI/L | rotule d'extrémité → condensation statique (3EI/L **et** qL²/8) |
| `MomemtEncastrement::rectangular` qL²/12 | encastrement parfait exact pour toute charge |
| Translations d'étage | DDL u, v de chaque nœud (sans hypothèse d'étage) |
| `Curves::Moment` = μ(x) + M_i(1 − x/L) + M_j x/L | identique, valeurs exactes + extrema/zéros par recherche de racine |
| — | déformée EI·v'' = M, flèche, rotations, réactions, effort normal |
| Hypothèse d'inextensibilité implicite | option `axialStiffnessFactor` |
