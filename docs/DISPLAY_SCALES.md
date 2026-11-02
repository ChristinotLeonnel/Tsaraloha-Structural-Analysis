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
| Échelle des charges | longueur des flèches de charge, d'après la structure (jamais le zoom) | Affichage > Échelle des charges ; Charges › Style des charges | automatique, facteur 1 |

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

`AnnotationStyle` (`src/Viewer/AnnotationStyle.*`) règle les textes de la vue : étiquettes de nœuds et d'appuis,
valeurs de diagrammes et de réactions, extremum, légendes et étiquettes de grille. Les cotations et les valeurs de
charges (style des charges, § 4) ont leur propre style.

| Mode | Hauteur | Au zoom |
| :--- | :--- | :--- |
| Taille constante à l'écran | pixels logiques × facteur d'écran (DPI), réglable de 8 à 32 px | inchangée |
| Taille liée au modèle | mètres du modèle × échelle de la vue, bornée à [8, 32] px | grandit et rétrécit avec la scène, réajustée à l'arrêt de la caméra |

- **Taille liée au modèle** (BUG-178, 2026-11-02) : jamais un texte OCCT « zoomable » (OCCT ne le trace pas sous ~16 unités du
  monde : un texte de 0,3 m sortait haut de ~11 m). La vue donne son échelle à l'arrêt de la caméra
  (`OccView::viewPixelsPerMeter` : médiane sur les nœuds visibles, car en perspective le zoom déplace le centre de visée ;
  `AnnotationStyle::setViewScale`, paliers de 15 %) ; les textes ne sont reconstruits que si leur hauteur change, et la
  caméra est conservée (la reconstruction complète recadrait la vue). Test 373.

- **Facteurs par rôle** : une légende ou un extremum est un peu plus grand qu'une étiquette de nœud.
- **« Adapter automatiquement »** : il calcule la taille en pixels d'après la hauteur de la vue, et la hauteur liée
  au modèle d'après l'envergure (envergure / 60).
- **Mémorisation** : les réglages sont enregistrés dans QSettings (`Display/Annotations/*`).
- **Effet d'un changement** : tous les textes de la vue sont reconstruits, sans objet orphelin, la caméra conservée
  (test 373). Le zoom ne reconstruit rien en taille constante ; en taille liée au modèle, une fois à l'arrêt de la caméra
  si la hauteur change.

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

- **Longueur de référence** : elle vient de la **structure**, jamais du zoom (`structureLoadReferenceLength`) : 35 % de
  la hauteur d'étage typique (médiane des éléments quasi verticaux), à défaut 30 % de la longueur médiane des barres,
  bornée à [1 %, 10 %] de l'envergure, puis multipliée par le facteur d'affichage. Sur un R+7 à étages de 3 m, la flèche
  maximale fait 1,05 m et reste sous le plancher supérieur. En zoomant, les flèches grandissent avec la scène :
  l'ancienne adaptation au zoom (longueur constante à l'écran, par paliers) est supprimée (2026-11-01).
- **Échelle robuste** (modes automatique et normalisé) : si le maximum d'une famille dépasse 3 fois son 90e centile, la
  famille est mise à l'échelle sur 3 × ce centile ; la charge hors norme est plafonnée à la longueur maximale (sa valeur
  reste affichée) et n'écrase plus les autres. Le mode proportionnel garde les rapports exacts au maximum.
- **Échelle commune à tous les cas** : les maxima sont relevés sur toutes les charges ; passer de G à Q ne change pas
  l'échelle, les flèches restent comparables.

### Style des charges (Charges › Style des charges…, à la manière d'un style de cote AutoCAD)

`LoadDisplayStyle` (`src/Viewer/LoadDisplayStyle.*`, réglage de l'application, `Display/Loads/*`), boîte
`UI/Display/LoadStyleDialog` (aperçu immédiat, Annuler rétablit, Valeurs par défaut) :

| Réglage | Choix | Défaut |
| :--- | :--- | :--- |
| Cas affichés | cas actif du projet (menu **Charges › Cas affichés**) ou tous les cas superposés | cas actif |
| Couleurs | par cas (10 couleurs distinctes), par intensité (bleu → rouge), par famille, unique | par cas |
| Texte | hauteur liée au modèle (m, automatique = 0,25 × longueur de référence, ou réglée), affichée entre deux bornes à l'écran ; ou constante à l'écran (8 à 32 px) | lié au modèle, automatique, 9 à 16 px |
| Valeurs superposées | masquées dans la vue, la plus forte reste ; la légende en donne le nombre | oui |
| Contenu du texte | décimales (0 à 4), symbole (« q = »), unité, nom du cas | 2, symbole, unité |
| Lisibilité | contour contrasté autour du texte (`Aspect_TODT_DEKALE`, pas de rectangle : BUG-122) | oui |
| Flèches | épaisseur × 0,5 à 3 | × 1 |

- **Cas actif** : `LoadManager::activeLoadCaseId`, enregistré dans le `.tsa` ; le changer par le menu redessine les
  charges sans changer la révision du modèle (résultats conservés) ni marquer le document modifié
  (`OccView::setDisplayedLoadCase`). La légende indique le cas affiché et le nombre de cas chargés masqués.
- Les textes des charges ne suivent plus `AnnotationStyle` (rôle LoadValue) mais ce style. Test 583.
- **Texte lié au modèle** (BUG-177, 2026-11-02) : jamais un texte OCCT « zoomable » — OCCT ne le trace pas sous ~16 unités
  du monde, et une valeur de 0,25 m sortait haute de ~11 m, recouvrant la structure. La hauteur en mètres (0,25 m pour un
  étage de 3 m : un texte de 2,5 mm sur un plan au 1/100) est convertie en pixels à l'échelle de la vue
  (`LoadDisplayStyle::labelPixels`), bornée à [9, 16] px × facteur d'écran : lisible en vue d'ensemble, jamais démesurée
  en vue rapprochée. Les valeurs sont dessinées au-dessus de la structure (calque `Topmost`, comme les cotes).
- **Mise en page** (`OccView::layoutLoadLabels`) : après chaque reconstruction des charges et 80 ms après l'arrêt de la
  caméra, sans recréer d'objet (hauteur des textes, `SetViewAffinity`). Les valeurs qui se chevauchent à l'écran sont
  masquées, la plus forte (|intensité| / maximum de la famille) restant visible ; en zoomant, elles réapparaissent. R+7
  3 × 3 travées, 192 valeurs : 55 lisibles en vue d'ensemble, 167 sur un zoom × 3.
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

Les repères (global, locaux, indicateur d'angle) ont eux aussi une taille constante à l'écran, réglée à part dans
**Affichage > Repères** : voir `docs/AXIS_DISPLAY.md`.

## 7. Profils d'affichage

**Affichage > Profils d'affichage** propose d'appliquer un profil, d'enregistrer l'affichage actuel comme profil,
de renommer ou supprimer un profil, et de rétablir les profils fournis. Le code est dans
`src/Viewer/DisplayProfile.*`.

- **Profils fournis** : ils sont créés au premier usage et restent modifiables comme les autres.
  - Modélisation ;
  - Charges ;
  - Maillage (représentation éléments finis) ;
  - Résultats ;
  - Déformées ;
  - Analyse modale ;
  - Présentation.
- **Contenu** : une **liste blanche de clés graphiques**.
  - représentation du modèle ;
  - couleurs (mode de rendu) ;
  - nœuds, numéros et appuis ;
  - grille, axes, niveaux et plan de travail ;
  - familles d'éléments visibles ;
  - charges : forces, moments, valeurs, mode et facteur d'échelle, longueur minimale, légende ;
  - déformée : mode et amplification ;
  - grandeur et multiplicateur des diagrammes, étiquettes ;
  - réactions, légende, rotation des sections ;
  - taille des annotations.

  Une clé non graphique (section, charge…) n'est jamais enregistrée. Appliquer un profil ne modifie ni le modèle
  (sa révision reste la même) ni les résultats (test 387).
- **Chargement tolérant** : une clé inconnue est ignorée et signalée dans la console. Une clé absente laisse le
  réglage courant tel quel. Une valeur hors bornes est refusée. Une liste illisible est chargée vide, sans échec.
- **Paramètres globaux et paramètres propres au projet** :
  - les profils sont des réglages **de l'application**, communs à tous les projets et conservés après redémarrage ;
  - le dernier profil appliqué à un projet enregistré lui est **associé** par le chemin du fichier, puis réappliqué
    à son ouverture ;
  - l'association est stockée dans les réglages : le format `.tsa` n'est pas modifié et les anciens fichiers restent
    compatibles ;
  - les clés de taille des annotations agissent sur le style d'annotation de l'application, commun à tous les
    onglets.
- **Synchronisation** : après application, les boutons et menus (grille, nœuds, appuis, charges, représentation)
  et le panneau des résultats relisent l'état de la vue.
