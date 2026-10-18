# Icônes de l'interface

## 1. Système

| Élément | Rôle |
|---|---|
| `resources/icons/**.svg` | Icônes vectorielles 24 × 24, dessinées pour le thème sombre. Nettes à toute échelle d'affichage de Windows. |
| `src/UI/Theme/ThemedIcon` | `TSA::UI::themedIcon(":/icons/…")` : toutes les icônes de l'interface passent par ce moteur. |
| `resources/icons/palette.json` | Couleurs remplacées selon le thème, générées par `tools/icons/make_palette.py`. |
| `tools/icons/generate_icons.py` | Icônes composées (glyphe + badge), générées et versionnées. |

### Rôle du moteur

- **Contraste** : au rendu, toute couleur de trait ou de marque sans contour qui ferait moins de **3:1** de contraste
  avec l'un des fonds du ruban est remplacée. Les fonds pris en compte sont le fond normal, le survol et le bouton
  actif, dans chaque thème. Elle est assombrie en thème clair et éclaircie en thème sombre, sans changer de teinte.
  Exemple : le jaune des cotations `#FFD54F`, presque invisible sur fond blanc, devient `#AB8200` en thème clair.
- **État désactivé** : gris atténué à 55 %, soit 2,3:1 à 3:1 avec le fond. La forme reste lisible, et l'état se
  distingue nettement de l'état normal.
- **Changement de thème** : il s'applique au prochain affichage, sans recréer les icônes. Les images sont mises en
  cache par taille, état et thème.
- **Logo du produit** : il garde ses couleurs, c'est une marque.

### États des boutons du ruban (ThemeManager)

| État | Repère |
|---|---|
| normal | icône et libellé |
| survol | fond et bordure |
| actif (commande en cours, option cochée) | fond, bordure, libellé en gras et **barre de 3 px sous le bouton**. Le repère est une forme, pas seulement une couleur, et il reste visible quand le bouton est réduit à son icône. |
| désactivé | icône grisée, libellé atténué mais lisible |

Chaque bouton du ruban a un **nom accessible**, le libellé de son action sans `&`, et une description, son infobulle.

## 2. Conventions de dessin

- 24 × 24, traits de 1,75 px (détails 1,5), extrémités et jonctions arrondies, fond transparent.
- Palette :

  | Couleur | Code | Usage |
  |---|---|---|
  | bleu | `#38BDF8` | objet principal, action |
  | gris ardoise | `#94A3B8` | contexte, structure secondaire |
  | rouge | `#EF4444` | charges, efforts appliqués |
  | vert | `#10B981` | ajout, validation, appuis |
  | ambre | `#F59E0B` | résultats, réglages |
  | violet | `#A78BFA` | maillage, IA |
  | rose | `#F43F5E` | suppression |

- **Badges** (coin inférieur droit) pour les variantes d'une même famille :

  | Badge | Sens |
  |---|---|
  | œil | afficher, masquer |
  | + | créer |
  | curseurs | paramètres |
  | flèche montante | exporter |
  | flèche descendante | importer |
  | coche | vérifier |
  | flèche circulaire | régénérer, réinitialiser |
  | cible | local, aligner |
  | × | supprimer |
  | aimant | accrochage |
  | flèches | suivant, précédent |
  | étiquette | numéros, libellés |

- Axes : X rouge, Y vert, Z bleu. Les lettres des diagrammes (Mz, My, Vz…) sont dessinées en traits, pas en texte.
- Pas de symbole Unicode ni de pictogramme 3D réaliste.

## 3. Audit et corrections (octobre 2026)

Inventaire de départ :

- 288 ressources, dont 97 jamais utilisées ;
- 92 groupes de fichiers au contenu identique (alias multiples) ;
- **22 groupes d'actions différentes avec la même image** ;
- 42 actions sans icône (des initiales générées les remplaçaient dans le ruban).

| Problème | Exemples | Correction |
|---|---|---|
| Même image, fonctions différentes | Paramètres utilisé pour Configuration IA, Gestionnaire de grilles, Raccourcis, Paramètres de modélisation et Plan de travail personnalisé ; « Nouveau projet » pour le Gestionnaire TSALib ; « Mesurer » pour « Vérifier la structure (IA) » ; Annuler et Rétablir pour Vue précédente et Vue suivante ; une seule image pour Réinitialiser la vue, Vue d'accueil et Vue normale au plan ; Exporter pour la Note de calcul | Une icône par fonction (86 nouvelles icônes). |
| Variantes impossibles à distinguer | Afficher la grille / Nouvelle grille ; Dessiner un nœud / Afficher les nœuds / Afficher les appuis ; Importer IFC / TSA3D / via module ; Pivoter à gauche / à droite | Même glyphe, badge ou sens distinct. |
| Plan de travail confondu avec une vue | Plan de travail XY = Vue de dessus, XZ = Vue de face, YZ = Vue de droite | Plans tramés (on y dessine), distincts des vues de caméra ; plans 2D avec leurs axes nommés. |
| Actions sans icône | 8 diagrammes d'efforts, 10 commandes de maillage, réactions, navigation du plan de travail, modules, signalement | Icônes dédiées. Les entrées de menus déroulants (options de collage, réglages fins de l'affichage du maillage) restent sans icône. |
| Contraste insuffisant | Jaunes, bleus clairs et blancs sur fond clair ; noirs sur fond sombre | Palette par thème : 39 couleurs adaptées au thème clair, 23 au thème sombre. |
| Alias trompeurs | `extension_manager.svg` et `common/diagnostic.svg` pointaient vers `settings.svg` ; `view/view_home.svg` vers le cube 3D ; `ndc_report.svg` vers Exporter | Alias corrigés. |
| Actif signalé par la seule couleur | Bouton réduit à son icône | Barre de 3 px. |

## 4. Contrôles automatiques (suite `i18n`)

| Test | Vérification |
|---|---|
| 460 | Toutes les icônes du catalogue des commandes existent et sont dessinées dans les deux thèmes. |
| 461 | Contraste mesuré pixel par pixel sur le fond du ruban : forme ≥ 3:1 en thèmes sombre et clair, état désactivé ≥ 1,8:1. |
| 462 | Deux commandes différentes n'ont jamais la même image (comparaison du contenu des fichiers). |
| 463 | Moteur d'icônes : palette par thème, état désactivé, ressource absente détectable. |

`python tools/icons/make_palette.py --check` signale une palette à régénérer après l'ajout d'une icône.

## 5. Ajouter une icône

1. Composer l'icône dans `tools/icons/generate_icons.py` avec les glyphes et badges existants, ou dessiner un SVG
   qui respecte le §2.
2. `python tools/icons/generate_icons.py`, puis `python tools/icons/make_palette.py`.
3. Déclarer le fichier dans `resources/resources.qrc` (alias = chemin relatif, par exemple `modeling/grid_new.svg`).
4. L'utiliser avec `TSA::UI::themedIcon(QStringLiteral(":/icons/…"))`, et la renseigner dans le catalogue
   (`CommandCatalog.cpp`), qui sert à la recherche et au Start Center.
5. Lancer la suite `i18n` : les tests 460 à 462 refusent une icône absente, illisible ou déjà prise par une autre
   commande.
