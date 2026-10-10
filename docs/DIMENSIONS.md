# Cotations 3D — TSA

Accès : ruban **Outils > Cotation**, ou menu **Modélisation > Cotation**. Les cotations sont des
**annotations** : elles ne font partie ni du calcul, ni de la masse, ni de la rigidité, ni des charges, ni du
maillage, et n'invalident pas les résultats (révision du modèle inchangée).

## Types

| Outil | Identifiant | Points | Valeur |
| :--- | :--- | :--- | :--- |
| Cotation alignée | `dim_aligned` | 2 + position | vraie grandeur entre les deux points (barre inclinée en 3D comprise) |
| Cotation linéaire | `dim_linear` | 2 + position | ΔX, ΔY ou ΔZ selon la position du curseur (`autoLinearAxis`) |
| Cotation horizontale | `dim_horizontal` | 2 + position | composante horizontale (plan XY) |
| Cotation X / Y / Z | `dim_x`, `dim_y`, `dim_z` | 2 + position | projection sur l'axe global |
| Cotation angulaire | `dim_angular` | sommet, bras 1, bras 2 + position | angle 0–180° dans le plan des trois points |
| Cotation de niveau | `dim_level` | 1 + position | Z − référence des niveaux (« +3.500 m », « ±0.000 m ») |
| Cotation en chaîne | `dim_chain` | ≥ 2, Entrée, position | longueurs successives sur une ligne de cote commune |
| Cotation cumulée | `dim_cumulative` | ≥ 2, Entrée, position | distances depuis le premier point (« 0 » à l'origine) |

Rayon / diamètre : **non proposés** — TSA n'a pas de géométrie circulaire (arcs, cercles, sections creuses
dessinées). Ils seront ajoutés avec une telle géométrie.

## Saisie

Les outils de cotation sont des outils du registre (`ToolCategory::Annotate`) : mêmes accrochages que le
dessin (nœuds, extrémités, milieux, grille, plan de travail), aperçu en direct, Échap pour annuler,
saisie clavier du **décalage** de la ligne de cote (le côté suit le curseur). Un point accroché à un nœud est
**associatif** ; un point de grille ou du plan de travail est un point fixe.

Après création : clic sur la ligne ou le texte → sélection (barre d'état : type, valeur) ; **Suppr** supprime ;
« Modifier la cotation » : position, texte (« <> » = valeur mesurée), couleur, nœuds associés ;
« Style des cotations » : unité, décimales, arrondi, unité affichée, décimales des angles, hauteur du texte,
taille des flèches, écart et dépassement des lignes d'attache, référence des niveaux, couleur, texte dans le
plan, affichage. Le style est enregistré dans le projet.

## Associativité et références invalides

La valeur est toujours recalculée depuis les coordonnées actuelles des nœuds (jamais stockée). Nœud déplacé
→ cotation mise à jour. Nœud supprimé → l'ancrage garde la dernière position connue, est marqué
`orphaned` et la cotation s'affiche en rouge avec « (réf. supprimée) » ; elle peut être réassociée
(« Modifier la cotation », numéro du nœud), convertie en point fixe (0) ou supprimée (« Supprimer les
cotations invalides »). Annuler la suppression du nœud rétablit l'association.

## Architecture

| Couche | Fichier | Rôle |
| :--- | :--- | :--- |
| modèle | `src/Annotation/Dimension.*` | entité (`Dimension`, `DimensionAnchor`), style (`DimensionStyle`), ensemble du projet (`DimensionSet`, JSON `schema` 1, lecture tolérante) |
| modèle | `src/Annotation/DimensionGeometry.*` | `computeLayout` : segments, flèches, textes, valeurs ; formatage (unités, arrondi, texte imposé) |
| modèle | `src/Annotation/DimensionService.*` | ajout, modification, suppression, réassociation, style — une entrée Annuler (`pushLabelUndoState`), notification `onDimensionsChanged` |
| vue | `src/Viewer/DimensionRenderer.*` | présentation OCCT : calque `Topmost`, flèches et texte à taille constante (`Graphic3d_TMF_ZoomPers`, `AIS_TextLabel` en pixels, fond), texte face à la caméra ou dans le plan |
| interaction | `src/Interaction/Tools/DimensionTools.*` | outils de saisie (10) |
| interface | `src/UI/Dialogs/DimensionDialogs.*`, `src/UI/MainWindow_Dimensions.cpp` | fenêtres, commandes, ruban |

`Model` stocke un `DimensionSet` (instantanés Annuler, `ModelDiff::dimensionsChanged`). `Model::removeNode`
marque les ancrages du nœud supprimé avant de notifier les vues. La sélection (`SelectionManager`,
`SelectionType::Dimension`) est exclusive avec celle des éléments en clic simple ; un élément du modèle détecté
sous le curseur est préféré à une cotation (`OccView::detectedPreferringModel`), si bien que les lignes
d'attache n'empêchent pas de sélectionner un nœud ou une barre.

## Persistance

Chunk `DIMS` (format 1.6), JSON UTF-8 de `DimensionSet` (cotations, style, prochain identifiant). Écrit s'il y
a des cotations ou un style non par défaut. Absent (projet ≤ 1.5) : aucune cotation, style par défaut. Une
cotation illisible est ignorée avec un avertissement, les autres sont chargées.

## Tests

Suite `dimensions` (tests 237-249) : JSON et lecture tolérante, alignée sur barre inclinée 3D, X / Y / Z /
horizontale et axe automatique, angulaire (plan incliné, angle plat, cas dégénérés), niveau, chaîne,
cumulée, unités / arrondi / texte imposé, associativité (déplacement, suppression, Annuler, réassociation,
nettoyage), exclusion du calcul (`CalculationSnapshot`), enregistrement / rechargement / ancien fichier,
outils de saisie, sélection cotation / barre, fenêtres, 2 000 cotations.
