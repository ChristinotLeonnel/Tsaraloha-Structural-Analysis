# CABLE & TENSION SYSTEM — VUE D'ENSEMBLE DU MODULE

Le module **Cable & Tension System** de **TSA (Tsaraloha Structural Analysis)** dote le logiciel d'une suite complète, modulaire et rigoureuse pour la modélisation, le dimensionnement, le dessin 3D interactif et l'analyse non linéaire des éléments tendus et câbles en génie civil.

---

## 1. Objectifs & Fonctionnalités Clés

1. **Typologie Complète de Câbles** :
   - Câbles génériques
   - Torons de précontrainte (Strands 7 fils)
   - Fils tréfilés (Wires)
   - Barres haute résistance à filetage continu (Prestressing Bars)
   - Haubans de ponts (Stay Cables)
   - Câbles porteurs paraboliques et caténaires (Suspension Cables)
   - Suspentes verticales ou inclinées (Hangers)
   - Câbles de précontrainte extérieure
   - Tirants d'ancrage au sol ou au rocher (Ground Anchors)

2. **Bibliothèque Normative Certifiée** :
   - Conformité stricte aux normes :
     - **EN 1993-1-11** (Eurocode 3 - Calcul des structures en câbles et tirants)
     - **EN 10138-3** (Aciers de précontrainte - Torons 7 fils Y1860S7, Y1770S7)
     - **EN 10138-4** (Aciers de précontrainte - Barres lisses et nervurées Y1030)
     - **EN 1992-1-1** (Eurocode 2 - Calcul du béton précontraint)
     - **ASTM A416 / AASHTO** (Seven-wire steel strand Grade 270)
   - Aucun coefficient ni valeur inventée : traçabilité stricte aux textes normatifs.

3. **Génération Solide 3D Haute Précision OpenCASCADE (B-Rep)** :
   - Représentation 3D volumique exacte aux diamètres réels métriques (ex: $\varnothing 15.7\text{ mm}$, $\varnothing 120\text{ mm}$).
   - Primitives cylindriques pour câbles droits rectilignes.
   - Balayage B-Rep (`BRepOffsetAPI_MakePipe`) sur courbes d'interpolation B-Spline (`GeomAPI_Interpolate`) pour les trajectoires courbes (paraboles, caténaires).
   - Têtes et culots d'ancrage orientés aux nœuds extrêmes.

4. **Interaction 3D & Dessin Intuitif dans le Viewport** :
   - Modes de tracé dédiés : Câble droit, Hauban, Câble porteur, Suspente.
   - Snapping automatique aux nœuds structuraux ou coordonnées de grille 3D.
   - Rubberband dynamique 3D en temps réel pendant le déplacement du curseur.

5. **Formulations Analytiques & Préparation à l'Analyse Non Linéaire** :
   - Calcul exact de la flèche, de la longueur curviligne et de la tension horizontale.
   - Équation de caténaire rigoureuse : $y(x) = c \cdot (\cosh((x - x_0)/c) - 1)$.
   - Module élastique équivalent sécant d'Ernst :
     $$E_{eq} = \frac{E}{1 + \frac{(w \cdot L)^2 \cdot E \cdot A}{12 \cdot T^3}}$$
   - Comportement en traction seule (*tension-only*) avec détection de mou (*slack cable*).
   - Pertes de tension par frottement ($\mu, k$) et par rentrée d'ancrage ($\Delta L_{slip}$).

6. **Intégration Totale dans TSA** :
   - Support complet de l'Undo/Redo différentiel via `ModelDiff` sans redessin complet de la scène.
   - Panneau de propriétés synchronisé en direct (`LiveSync`) avec bascule manuelle Valider/Annuler.
   - Sélection unifiée dans `SelectionManager` et `ModelTreeWidget`.
   - Sauvegarde et chargement binaire pérenne dans le format `.tsa` (chunk `CHUNK_CABL`).
