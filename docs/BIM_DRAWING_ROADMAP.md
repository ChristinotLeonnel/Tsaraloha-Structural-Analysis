# Feuille de route — outils de dessin BIM

Branche `feature/bim-drawing`. Constat : `BIM_DRAWING_AUDIT.md` (problèmes D1 à D13).
Architecture : `BIM_ARCHITECTURE.md` § 5. Bilan : `BIM_DRAWING_IMPLEMENTATION_REPORT.md`.

L'ordre suit celui de la mission. Il est adapté là où l'existant était déjà conforme : modèle
métier, identifiants, accrochage et IFC existaient, avec des tests. Le travail porte donc d'abord
sur le chemin de création, qui était le maillon défaillant (D1 à D7).

| # | Étape | Contenu | État |
| :--- | :--- | :--- | :--- |
| 1 | Modèle métier | `Model` + `BimModel` (produit physique ↔ éléments analytiques) | existant, conservé |
| 2 | Identifiants et relations | GlobalId des produits et objets analytiques, mapping 1:N | existant ; vérifié pour les éléments dessinés (test 417) |
| 3 | Commandes transactionnelles | `ElementCreation` : validation, puis nœuds + élément dans une `EditTransaction` | **fait** (tests 410-416, 419) |
| 4 | Accrochage | `SnapEngine` seul moteur ; coïncidence de nœud centralisée (`GeometryTolerance::nodeCoincidence`) | **fait** pour la tolérance ; `GridSnapManager` (D13) à retirer |
| 5 | Outils structuraux | modes Dessin de la vue (nœud, barre, poutre, poteau, dalle, voile, fondation, treillis, haubans, câbles porteurs, suspentes), dialogues Barre / Câble / Surface, préréglages fondation et treillis | **fait** ; outils `ModelingTool` de dessin (chaîne, rectangle, portique…) : déjà transactionnels, à faire passer par les validations du service |
| 6 | Palette de propriétés | identité BIM et quantités de l'élément (`ElementBimPanel`) | **fait** (test 422) |
| 7 | Cohérence des vues | navigateur avec hiérarchie spatiale BIM (projet, bâtiment, étages, produits) | **non commencé** (D10) |
| 8 | Sauvegarde et migration | aucun champ persistant ajouté : format `.tsa` inchangé (1.11), fichiers existants lus tels quels ; aller-retour vérifié | **fait** (test 423) ; préréglages non persistés (comme avant) |
| 9 | Adaptateur analytique | le modèle analytique reste celui de TSA ; résultats périmés à la création | existant ; vérifié (test 418) |
| 10 | IFC | export / réimport des éléments dessinés | existant ; vérifié (test 423) |
| 11 | Quantités | `QuantityTakeoff` dérivé du modèle | **fait** (tests 420, 421) |

## Reste à faire (par priorité)

1. **Outils `ModelingTool` de dessin** : ils passent par `ToolGeometry::nodeAt` dans une transaction,
   mais sans les validations ni les avertissements de croisement du service. Il faut les faire
   passer par `ElementCreation` sans changer leur ergonomie.
2. **Navigateur BIM (D10)** : branche « Projet › Bâtiment › Étages › produits » dans
   `ModelTreeWidget`, synchronisée avec la sélection.
3. **Bibliothèque de types (D11)** : section et matériau partagés par référence (type de produit),
   avec propagation et migration du format.
4. **Grilles dans le `Model` (D12)**, puis retrait de `GridSnapManager` (D13).
5. **Préréglages persistés** dans les paramètres utilisateur (aujourd'hui remis à zéro à chaque
   lancement, comme avant ce chantier).
6. **Essai de l'interface réelle** (UI Automation) du tracé complet dans TSA, en complément du
   test 419, qui pilote déjà une vraie vue OCCT par clics.
7. ~~Voiles à base inclinée~~ : **fait** le 18/10/2026 (calcul, métré, export et réimport IFC vérifiés ensemble,
   tests 428 et 429). Reste : porte au pied d'un voile à base inclinée.
8. Libellés des matériaux des dialogues : passer par la bibliothèque (`MaterialLibrary`) plutôt
   que par des noms recherchés par mot-clé.
