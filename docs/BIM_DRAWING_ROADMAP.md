# Feuille de route — outils de dessin BIM

Branche `feature/bim-drawing`. Constat : `BIM_DRAWING_AUDIT.md` (problèmes D1 à D13).
Architecture : `BIM_ARCHITECTURE.md` § 5. Bilan : `BIM_DRAWING_IMPLEMENTATION_REPORT.md`.

L'ordre suit celui de la mission. Il est adapté là où l'existant était déjà conforme : modèle
métier, identifiants, accrochage et IFC existaient, avec des tests. Le travail a donc d'abord
porté sur le chemin de création, qui était le maillon défaillant (D1 à D7), puis sur la feuille
de route validée le 18/10/2026.

| # | Étape | Contenu | État |
| :--- | :--- | :--- | :--- |
| 1 | Modèle métier | `Model` + `BimModel` (produit physique ↔ éléments analytiques) | existant, conservé |
| 2 | Identifiants et relations | GlobalId des produits, objets analytiques et types ; mapping 1:N | existant ; vérifié (tests 417, 430) |
| 3 | Commandes transactionnelles | `ElementCreation` : validation, puis nœuds + élément dans une `EditTransaction` | **fait** (tests 410-416, 419) |
| 4 | Accrochage | `SnapEngine` seul moteur ; coïncidence de nœud centralisée | **fait** ; `GridSnapManager` (D13) à retirer |
| 5 | Outils structuraux | modes Dessin, dialogues Barre / Câble / Surface, six outils avancés, préréglages fondation et treillis | **fait** (tests 410-416, 425) |
| 6 | Palette de propriétés | identité BIM, type et quantités de l'élément | **fait** (tests 422, 426) |
| 7 | Cohérence des vues | branche « Structure BIM » de l'arbre (site, bâtiment, étages, produits, types) | **fait** (test 427) |
| 8 | Sauvegarde et migration | format `.tsa` inchangé ; types en champs facultatifs du bloc BIM ; création des types depuis un projet existant | **fait** (tests 423, 430) ; préréglages non persistés (comme avant) |
| 9 | Adaptateur analytique | modèle analytique de TSA ; résultats périmés à la création et à la propagation d'un type | existant ; vérifié (tests 418, 431) |
| 10 | IFC | éléments dessinés, voiles inclinés, types de produits | **fait** (tests 423, 428, 432) |
| 11 | Quantités | `QuantityTakeoff` dérivé du modèle | **fait** (tests 420, 421, 428) |
| 12 | Voiles à base inclinée | calcul, métré, IFC cohérents | **fait** (tests 424, 428, 429) |
| 13 | Interface réelle | `tools/ui_tests/drawing_scenario.ps1`, 17 vérifications | **fait** |

## Reste à faire (par priorité)

1. **Éditeur de types** : aujourd'hui, on modifie une occurrence puis on propage au type. Une
   fenêtre « Types de produits » (section, matériau, épaisseur ; occurrences ; écarts) rendrait
   l'opération directe.
2. **Porte au pied d'un voile à base inclinée** : aujourd'hui refusée (seules les ouvertures
   intérieures sont prises en charge).
3. **Grilles dans le `Model` (D12)**, puis retrait de `GridSnapManager` (D13).
4. **Préréglages persistés** dans les paramètres utilisateur (aujourd'hui remis à zéro à chaque
   lancement, comme avant ce chantier).
5. **Libellés des matériaux des dialogues** : passer par la bibliothèque (`MaterialLibrary`) plutôt
   que par des noms recherchés par mot-clé.
6. **CI** : lancer en plateforme `windows` les quatre suites qui créent une vue OCCT (BUG-076).
