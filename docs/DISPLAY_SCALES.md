# Échelles d'affichage — TSA

Règle de base : **une échelle graphique ne modifie jamais une valeur physique.** Une charge de 100 kN reste
égale à 100 kN dans le modèle, dans le fichier et dans le calcul, même si sa flèche est raccourcie pour tenir à
l'écran. Agrandir un diagramme de moments agrandit son dessin, pas le moment calculé. Les tests 372, 374 et 376
vérifient que les valeurs restent identiques au bit près après chaque changement d'échelle.

## 1. Les cinq échelles

Elles sont indépendantes. Changer l'une ne modifie aucune des autres.

| Échelle | Effet | Réglage | Valeur par défaut |
| :--- | :--- | :--- | :--- |
| Caméra (zoom) | taille de toute la scène à l'écran | molette, boutons de vue | cadrage du modèle |
| Amplification de la déformée | multiplie les déplacements dessinés | panneau Résultats, « Amplification de la déformée » | automatique (déplacement max ≈ 5 % de l'envergure) |
| Échelle des diagrammes | m de dessin par unité de la grandeur (kN·m, kN, mm, rad) | panneau Résultats, « Échelle des diagrammes » : Automatique, ×0,5, ×2, ×5, Personnalisé | automatique |
| Taille des annotations | hauteur des textes (valeurs, étiquettes, légendes) | Affichage > Échelle des annotations | 12 px, constante à l'écran |
| Échelle des charges | longueur des flèches de charge | Affichage > Échelle des charges | automatique, facteur 1 |

## 2. Diagrammes de résultats

- **Échelle automatique** : la plus grande valeur absolue de la grandeur occupe
  min(8 % de l'envergure, 60 % de la longueur médiane des éléments). Cette échelle est sans plafond, ce qui garde
  lisibles les grandeurs faibles (rotations en rad, flèches en mm).
- **Multiplicateur choisi par l'utilisateur** : il est appliqué par-dessus l'échelle automatique. Il est conservé
  d'un calcul à l'autre et d'une grandeur à l'autre. Les préréglages sont des multiplicateurs, pas des échelles
  absolues.
- **Grandeur nulle partout** (par exemple aucune torsion) : aucun diagramme n'est dessiné. La légende indique
  « grandeur nulle sur toute la structure ».
- **Valeurs affichées** : 4 chiffres significatifs (0,005912 rad n'apparaît plus comme « 0.0 rad »). À un nœud
  partagé, une étiquette proche de moins de 4 % de l'envergure d'une étiquette de valeur absolue supérieure est
  masquée. La légende indique le nombre d'étiquettes masquées ; les valeurs restent dans les tableaux et la note.
- **Légende** : elle est fixée à l'écran, dans le coin inférieur droit, et ne suit pas la caméra. Sa première ligne
  indique le moteur, le chargement et l'heure du calcul affiché. En analyse modale, elle indique le mode et sa
  fréquence ; en non-linéaire, le pas.

## 3. Annotations

`AnnotationStyle` (`src/Viewer/AnnotationStyle.*`) est le point de réglage unique de tous les textes de la vue :
étiquettes de nœuds et d'appuis, valeurs de charges, de diagrammes et de réactions, extremum, légendes et
étiquettes de grille. Les cotations gardent leur propre style.

| Mode | Hauteur | Au zoom |
| :--- | :--- | :--- |
| Taille constante à l'écran | pixels logiques × facteur d'écran (DPI), réglable de 8 à 32 px | inchangée |
| Taille liée au modèle | mètres du modèle (texte « zoomable ») | grandit et rétrécit avec la scène |

- **Facteurs par rôle** : une légende ou un extremum est un peu plus grand qu'une étiquette de nœud.
- **« Adapter automatiquement »** : il calcule la taille en pixels d'après la hauteur de la vue, et la hauteur liée
  au modèle d'après l'envergure (envergure / 60).
- **Mémorisation** : les réglages sont enregistrés dans QSettings (`Display/Annotations/*`).
- **Effet d'un changement** : tous les textes de la vue sont reconstruits, sans objet orphelin (test 373). Le zoom
  ne reconstruit rien.

## 4. Charges

`LoadDisplayScale` (`src/Geometry/LoadDisplayScale.*`) calcule la longueur de chaque flèche. Les charges sont
comparées seulement à l'intérieur d'une même famille d'unités : forces (kN), moments (kN·m), charges linéiques
(kN/m), moments linéiques (kN·m/m) et pressions (kN/m²). Une charge linéique de 500 kN/m n'écrase donc pas les
forces nodales.

| Mode | Longueur |
| :--- | :--- |
| Proportionnelle | intensité / maximum de la famille × longueur de référence ; minimum 3 % pour qu'une flèche reste visible |
| Automatique (défaut) | proportionnelle, avec un minimum réglable (25 % par défaut) |
| Normalisée | même longueur pour toutes ; l'intensité est donnée par la valeur affichée |

- **Longueur de référence** : elle dépend de la taille du modèle et du facteur d'affichage.
- **Nouvelle charge plus forte que le maximum** : toute la famille est remise à l'échelle. La reconstruction est
  différée, ce qui évite d'en lancer une par charge.
- **Légende des charges** : elle est fixée à l'écran, en haut à gauche, et donne, pour chaque famille, le maximum
  et la longueur de flèche correspondante.

## 5. Diagnostic d'affichage

**Affichage > Diagnostic d'affichage…** s'applique au document au premier plan. Il affiche, et copie dans la
console :
- le nombre d'objets de la scène ;
- les objets de résultats, par type ;
- toutes les échelles en vigueur, avec la longueur de flèche du maximum de chaque famille de charges.

La fonction est `OccView::displayDiagnosticsText()` (test 376). Un objet de la scène qui n'apparaît dans aucun
compte serait un objet orphelin.

## 6. Résultats d'un ancien calcul (corrigé)

Trois causes faisaient réapparaître des courbes d'un ancien calcul. Elles sont corrigées et couvertes par le test
371, qui échouait sur l'ancien code.

1. Les objets de résultats étaient rangés par identifiant d'élément seul. Or la poutre 1 et le poteau 1
   coexistent : le second écrasait le premier, qui restait affiché dans la scène sans plus être suivi.
   La clé est désormais `ElementKey {famille, id}`, et un objet remplacé est retiré de la scène.
2. Le marqueur d'extremum de la note de calcul n'était pas effacé au changement de résultats.
3. L'ouverture d'un fichier ne vidait pas les résultats du projet précédent.

Depuis les onglets multi-projets (`docs/MULTI_DOCUMENT.md`), chaque document a sa propre vue et ses propres
résultats. Un nouveau projet s'ouvre dans un onglet neuf.
