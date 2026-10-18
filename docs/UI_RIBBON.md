# Ruban de TSA

Le ruban suit l'ordre du travail de l'ingénieur en structure. Chaque commande y a **une seule place**. La barre d'accès
rapide (Sélection, Cadrer tout, Vue 3D, Lancer le calcul) et le menu de l'application (Quitter, projets récents) font
exception. Toutes les commandes restent aussi accessibles par la recherche globale (`Ctrl+F`,
[UI_SEARCH.md](UI_SEARCH.md)) et par leurs raccourcis.

## 1. Onglets et groupes

| Onglet | Groupes (commandes principales en grand, variantes en colonnes) |
|---|---|
| **Accueil** | Projet (Nouveau · Ouvrir, Enregistrer, Enregistrer sous · Fermer) · Historique et presse-papiers · Échanges BIM (Importer IFC · Exporter IFC, TSA3D, import par module, couche BIM) · Paramètres du projet (Topologie et numérotation · Paramètres de modélisation) · Préférences (Langue · Thème, Raccourcis) |
| **Modélisation** | Sélection · Éléments filaires (Poutre, Poteau · Barre, Câble, Treillis) · Surfaces (Dalle, Voile · Semelle, Ouvertures) · Nœuds · Dessin rapide (outils du registre) · Sections et matériaux · Appuis (Encastrement · Articulation, Appui simple, Appuis linéiques) |
| **Modifier** | Déplacer et copier · Symétrie et topologie (Symétrie · Diviser, Fusionner, Nettoyer) · Outils de modification (saisie 3D, échelle, réseaux, décalage, couper, ajuster, prolonger) · Suppression |
| **Grille et plans** | Grilles et niveaux · Plan de travail (XY · XZ, YZ, étage · personnalisé, afficher, vue normale) · Navigation sur la grille (plan suivant / précédent / retour · intersection suivante / précédente / la plus proche) · Accrochage · Aides visuelles (grille, niveaux, axes, règles, échelle des annotations) · Repère et coupes |
| **Charges** | Nœud · Barre (uniforme, trapézoïdale, ponctuelle) · Surface · Poids propre · Cas et combinaisons · Affichage des charges (charges, forces, moments, valeurs, charges surfaciques, échelle) |
| **Maillage** | Génération (générer · régénérer, supprimer) · Paramètres (général · local, raffinement) · Contrôle (aperçu, qualité) · Affichage (afficher · menu d'affichage du maillage) · Échanges (exporter, importer) |
| **Analyse** | Calcul (Lancer le calcul, Paramètres d'analyse) · Vérification (IA) |
| **Résultats** | Panneau · Déformée et efforts (déformée · déplacements, efforts internes, contraintes) · Diagrammes (Mz, My, Mx, Vz, Vy, N, flèches, masquer) · Réactions · Cadrage · Rapports (note de calcul · documents par modèles, explication IA) |
| **Affichage** | Vues (3D · plans 2D XY, XZ, YZ · dessus, dessous, face, arrière, gauche, droite · isométrique, accueil, vues nommées) · Navigation · Représentation · Rendu · Visibilité · Fenêtres |
| **Outils** | Mesure · Cotation · Assistant IA · Extensions (TSALib · modules) · Aide (aide · documentation en ligne, raccourcis, à propos · signaler un problème, diagnostic) |

## 2. Changements par rapport à l'ancien ruban (9 onglets)

**Doublons supprimés.** L'Accueil reprenait Structure, Appuis et charges, Analyse et Vue. Les appuis figuraient dans
Modèle et dans Structure. Le panneau Résultats apparaissait dans Analyse et dans Résultats, l'IA dans Analyse et dans
Outils, et le presse-papiers dans Accueil et dans Modifier.

**Regroupements.**
- Grille, niveaux, plan de travail et accrochage, jusque-là répartis entre Modèle et Affichage, forment l'onglet
  **Grille et plans**. Les commandes de navigation sur la grille y entrent ; elles n'étaient accessibles que par menu
  et raccourci.
- Le maillage quitte Analyse pour son propre onglet **Maillage**, avec ses commandes d'export, d'import et de
  suppression, qui n'étaient pas dans le ruban.
- Sections, matériaux et bibliothèque rejoignent Modélisation. L'onglet Structure disparaît.

**Commandes ajoutées au ruban** : vues de dessous et arrière, rotation de la vue 2D, efforts internes et contraintes,
modèles de documents, Tout sélectionner, ouvertures de dalle, affichage des appuis et de leurs étiquettes, charges
surfaciques, modules, signaler un problème, export du diagnostic, langue, éditeur de raccourcis.

**Plus aucun bouton de remplacement.** L'ancien constructeur créait une action sans effet (« Profilé I/H », « Calcul
statique », « Note de calcul »…) quand la fenêtre ne fournissait pas la commande. Le ruban ne pose plus que les actions
réelles de la fenêtre, et un groupe sans commande n'est pas affiché.

**Onglet BIM et extensions.** Proposé par le cahier des charges, il n'aurait contenu que quelques commandes. Les
échanges BIM sont dans l'Accueil (Importer, Exporter), et les extensions et modules dans Outils.

## 3. Adaptation à la largeur

`RibbonTab` adapte chaque onglet à la place disponible, sans défilement horizontal, en quatre étapes :

1. groupes complets ;
2. petits boutons réduits à leur icône, avec infobulle et nom accessible ;
3. groupes repliés en un bouton déroulant, de droite à gauche ;
4. derniers groupes dans le menu « Plus ».

Aucune commande n'est jamais retirée : le test 466 vérifie que chaque commande reste atteignable de 1920 à 420 px.
La rangée d'onglets mesure au plus 775 px, en malgache (test 467), et tient sur un écran de portable de 1366 px.

## 4. Accessibilité et essais

- Chaque bouton a un **nom accessible** (le libellé de son action) et une description (son infobulle).
- L'état actif est marqué par une barre de 3 px, pas seulement par une couleur ([ICONS.md](ICONS.md)).
- Chaque bouton porte l'identifiant de sa commande comme nom d'objet, par exemple `cmd.display.grid`. C'est l'AutomationId
  UI Automation. Les essais de `tools/ui_tests` retrouvent ainsi une commande quelle que soit la langue.

Tests : suite `ribbon` (464 à 467) et essai d'interface `tools/ui_tests/i18n_scenario.ps1`.

## 5. Ajouter une commande au ruban

1. Créer l'action dans `MainWindow` et la relier à son identifiant du catalogue (`MainWindow_Shortcuts.cpp`).
2. Ajouter un champ dans `RibbonActions`, le remplir dans `MainWindow::createRibbon()` et le poser dans le groupe voulu
   de `RibbonBuilder.cpp`.
3. L'ajouter à `RibbonBuilder::placedActions()`. Le test 464 refuse une commande non placée, placée deux fois ou un
   bouton sans action réelle.
