# Éléments surfaciques — dalles et voiles

Ce document décrit la prise en charge des éléments surfaciques dans TSA. Il couvre :

- la modélisation ;
- les ouvertures ;
- le maillage ;
- les charges ;
- le calcul ;
- les résultats et la note de calcul.

Il précise aussi les hypothèses et les limites. Les fonctions décrites sont vérifiées par la suite de tests `surfaces` (tests 354 à 358).

## 1. Architecture

```
Modèle TSA : Slab / Wall (géométrie, épaisseur, matériau, formulation, ouvertures)      SurfaceLoad (LoadManager)
        │                                                                                  │
MeshGeometry::extract / wallSurface (contours, trous, repère local) ── vue 3D (même contour)
        │
Gmsh (maillage conforme : arêtes communes partagées, nœuds TSA plongés)  →  FeMesh
        │
OpenSeesMeshAdapter::check(…, model) : formulation, ordre, connexions, poids propre, charges → SurfaceAnalysisModel
        │                                                     (analyse indépendante du mailleur)
AnalysisContext.surfaces → CalculationSnapshot::withSurfaces (nœuds partagés, coques, forces nodales)
        │
OpenSeesAnalysisBuilder : ElasticMembranePlateSection, ShellMITC4 / ShellDKGT, load, recorder « stresses »
        │
ResultsModel : ShellElementResult (par élément du maillage), déplacements par nœud du maillage
        │
ResultsModelMeshAdapter (champs sur le maillage) · NDCProjectNote (note de calcul) · OccView (charges, ouvertures)
```

Les responsabilités sont séparées comme suit :

- **Objets de modélisation** (`Slab`, `Wall`, `SurfaceLoad`) : ils ne dépendent d'aucun moteur.
- **Géométrie maillable** (`MeshGeometry`) : elle reprend le même contour pour l'affichage.
- **Maillage** (`FeMesh`) : il identifie ses nœuds et éléments par des identifiants propres (`FeNode::id`, `FeElement::id`), sans lien avec les nœuds de modélisation.
- **Adaptateur par moteur** (`OpenSeesMeshAdapter`) : c'est le seul composant qui connaît les éléments OpenSees.
- **Analyse** (`SurfaceAnalysisModel`) : elle ne reçoit que des nœuds, des éléments de coque et des forces nodales déjà converties.

## 2. Formulations mécaniques

La formulation se règle dans les propriétés de la dalle ou du voile.

| Formulation | Comportement | OpenSees (TSA, `-ndm 3 -ndf 6`) |
|---|---|---|
| Coque (par défaut) | membrane (N) + plaque (M, V) | **calculée** : `ShellMITC4` (quadrangle, Mindlin-Reissner à cisaillement interpolé), `ShellDKGT` (triangle, Kirchhoff discret), section `ElasticMembranePlateSection` (E, ν, e, ρ) |
| Plaque seule | flexion hors plan, sans membrane | **refusée avant le calcul** : OpenSees n'a pas d'élément de plaque sans membrane en 3D |
| Membrane seule | efforts dans le plan, sans flexion | **refusée avant le calcul** : `quad` et `tri31` ont 2 DDL par nœud, incompatibles avec le modèle à 6 DDL. Une coque de faible rigidité de flexion serait une rigidité fictive |
| Non structurale | géométrie seulement | non transmise (avertissement) ; une charge sur cette surface bloque le calcul |

Aucune formulation n'est remplacée silencieusement par une autre. Les éléments du second ordre (tri6, quad8) sont eux aussi refusés : il faut régénérer le maillage au premier ordre.

Quatre axes d'extension sont prévus :

- **Formulations :** de nouvelles formulations s'ajoutent à `SurfaceFormulation`, et chaque adaptateur déclare celles qu'il calcule.
- **Comportement non linéaire :** les matériaux non linéaires ou la fissuration du béton demandent un moteur adapté et une section stratifiée (`LayeredShell`) dans l'adaptateur.
- **Analyses de stabilité et dynamique :** elles relèvent des moteurs, pas du modèle.
- **Éléments d'ordre supérieur :** `ShellMITC9` exige un maillage quad9, que l'adaptateur ne produit pas aujourd'hui.

## 3. Ouvertures

Les ouvertures se définissent dans le dialogue « Ouvertures (dalle ou voile) », dans le repère local de la surface :

- **Dalle :** l'origine est au premier nœud, u est orienté vers le deuxième nœud, v est perpendiculaire dans le plan.
- **Voile :** u court le long du voile depuis le nœud de départ, v monte depuis le pied.

| Type | Traitement |
|---|---|
| Rectangle, cercle (polygone à 24 côtés), polygone libre | trou du contour (maillage, aire nette, affichage) |
| Porte : rectangle d'un voile posé au pied (v = 0) | **encoche du contour** du voile. Elle ne peut pas être un trou, puisqu'elle touche le bord |

La validation (`validateSlabOpening`, `validateWallOpening`) refuse les ouvertures suivantes :

- une ouverture qui sort de la surface ;
- une ouverture qui recoupe le contour ;
- une ouverture qui chevauche ou contient une autre ouverture ;
- deux portes qui se touchent.

Une ouverture **retire la matière** à toutes les étapes :

- aucun élément fini ne la recouvre ;
- l'aire nette sert au poids propre et aux charges ;
- la vue 3D affiche le voile ou la dalle découpés.

Une charge ponctuelle placée dans une ouverture est refusée. Les ouvertures sont enregistrées dans le chunk `OPNG` du `.tsa` (clés `slabs` et `walls`).

## 4. Maillage

Le maillage est produit par Gmsh (voir `docs/MESH.md`). Il offre :

- triangles, quadrangles par recombinaison, ou maillage mixte ;
- taille globale et tailles locales ;
- raffinement autour des ouvertures, des appuis et des charges ;
- contrôle de qualité ;
- aperçu avant génération.

Le maillage est **conforme par construction**. Une arête commune entre une dalle et une poutre, ou entre deux dalles, est une seule courbe du mailleur. Les nœuds TSA situés dans une dalle y sont plongés.

Au calcul, `CalculationSnapshot::withSurfaces` rattache chaque nœud de coque :

- au nœud TSA qu'il représente ;
- sinon, au nœud du calcul situé au même point (à 1 µm près), par exemple un nœud de subdivision de la poutre qui porte l'arête ;
- sinon, à un nœud propre à la surface.

Ce rattachement ne fusionne jamais de nœuds éloignés. Les interfaces non conformes relevées par le mailleur sont signalées avant le calcul.

## 5. Charges surfaciques

Les charges s'appliquent avec l'action « Charge surfacique » sur une ou plusieurs dalles ou voiles. Elles sont annulables.

| Répartition | Valeur | Directions |
|---|---|---|
| Uniforme sur la surface nette | kN/m² | gravité (−Z), X, Y, Z global, normale locale (−N : une valeur positive appuie sur la face) ; surface projetée possible pour une direction globale |
| Ponctuelle en (u, v) | kN | mêmes directions |

La conversion est faite par `SurfaceLoads`, sur le maillage :

- **Charge uniforme :** elle est convertie en charges nodales cohérentes ∫ Nᵢ p dA, par intégration de Gauss sur les fonctions de forme de chaque élément. La répartition n'est donc pas faite à parts égales entre les nœuds.
- **Charge ponctuelle :** elle est répartie par les fonctions de forme de l'élément qui contient le point, soit Nᵢ(ξ, η) · F.
- **Contrôle :** la résultante et le moment par rapport à l'origine sont comparés à la valeur analytique (aire nette et centre de surface). Un écart bloque la conversion.

Les charges sont enregistrées dans le chunk `SURF` (format 1.10). Elles disparaissent avec leur surface.

Trois cas bloquent le calcul :

- une charge sur une surface non transmise (non maillée, non structurale ou de formulation refusée) ;
- une charge surfacique alors qu'aucun maillage n'est transmis.

Une charge n'est jamais ignorée en silence.

**Affichage :**

- **Représentation :** une grille de flèches couvre la zone nette (ouvertures exclues), avec le contour de la zone chargée.
- **Étiquette :** elle donne la valeur, l'unité, le cas et le repère.
- **Visibilité :** elle se règle par « Afficher les charges surfaciques » (Affichage > Aides visuelles).

Les **coefficients des combinaisons** viennent des combinaisons du projet : aucun coefficient réglementaire n'est codé en dur.

## 6. Poids propre

Le poids propre d'une coque vaut ρ · g · e (g = 9,81 m/s²) sur l'aire nette. Il est réparti en charges nodales cohérentes et appliqué aux cas qui incluent le poids propre, avec leur facteur.

Il est calculé automatiquement. Il ne faut donc pas le saisir en plus comme charge surfacique : le dialogue le rappelle. Les dalles non transmises n'ont pas de poids propre calculé.

## 7. Connexions avec les barres, appuis et fondations

Les connexions suivent ces règles :

- **Dalle et poutre :** les nœuds sont partagés le long de l'arête commune. La continuité des déplacements et des rotations est assurée par les 6 DDL communs.
- **Plans moyens et excentricités :** le plan moyen de la coque coïncide avec l'axe de la barre. **Aucune excentricité ni liaison rigide n'est ajoutée** (les retombées de poutre ne sont pas modélisées).
- **Dalle et poteau, voile et dalle :** les nœuds TSA communs sont partagés, et les nœuds plongés dans la dalle aussi.
- **Surface isolée** (aucun nœud commun avec une barre, un appui ou une autre surface) : **refusée** (mécanisme). Un voile excentré (`offset` non nul) n'a pas de nœud commun avec ses nœuds de pied : il doit être relié explicitement.
- **Appuis :** ils ne s'appliquent qu'aux nœuds TSA. Un bord de dalle « appuyé » demande des nœuds TSA d'appui le long du bord, à un pas inférieur à la taille de maille. Sinon, des nœuds de maillage intermédiaires restent libres. Les tests 354 et 356 le vérifient.
- **Fondations :** les semelles ne sont pas transmises (volumes à 3 DDL). Les appuis aux nœuds restent appliqués.

## 8. Résultats

Les résultats suivants sont disponibles :

- **Résultantes de coque :** elles sont relevées aux 4 points d'intégration et sous forme de moyenne par élément, dans le repère local de l'élément (axe x du nœud 1 vers le nœud 2). Ce sont Nxx, Nyy, Nxy (kN/m), Mxx, Myy, Mxy (kN·m/m), Vxz, Vyz (kN/m), plus les valeurs principales, indépendantes de ce repère.
- **Déplacements :** ils sont exacts à chaque nœud du maillage.
- **Affichage :** Analyse > Maillage > « Déplacements sur le maillage », puis « Grandeur affichée sur le maillage ». La déformée est amplifiée et le champ est coloré. Ces résultats ne sont reportés que sur le maillage qui a servi au calcul (empreinte de génération).
- **Équilibre global :** il intègre les charges surfaciques et le poids propre des coques.
- **Note de calcul** (simplifiée et détaillée) : elle reprend les hypothèses des coques, les charges surfaciques et les résultantes maximales par surface. Son contrôle indépendant de la charge verticale inclut les surfaces. Le ferraillage des dalles et voiles et le poinçonnement y sont marqués « non calculée ».

Conventions de signe (OpenSees, repère local de l'élément) : une plaque fléchie vers le bas sous une charge descendante présente des moments M négatifs en travée.

## 9. Validation (suite `surfaces`)

| Test | Cas | Résultat |
|---|---|---|
| 354 | bande en console 4 × 1 m, e = 0,20 m, ν = 0, q = 10 kN/m², comparée à qL⁴/8EI | quadrangles 0,42 % (h = 0,5 m) puis 0,36 % (h = 0,25 m) ; triangles 0,12 % ; Σ R = qLb ; moment d'encastrement qbL²/2 exact ; champs sur le maillage |
| 355 | dalle 6 × 4 m avec trémie 1 × 1 m ; voile 5 × 3 m avec porte et fenêtre | aucun élément dans les ouvertures ; Σ R = poids propre (aire nette) + q·A nette + P ; aire maillée = aire nette |
| 356 | dalle sur 4 poutres et 4 poteaux ; voile de contreventement en console | nœuds de bord partagés avec les poutres ; Σ R = charges + poids propre ; voile à 2,8 % de Timoshenko ; note de calcul |
| 357 | plaque, membrane, non structurale, surface isolée, charges non transmissibles | refus explicites |
| 358 | enregistrement et relecture | formulations, ouvertures des voiles, charges surfaciques identiques |

Une plaque de Navier (4 × 4 m, appuis simples sur les quatre bords) calculée hors de TSA avec les mêmes éléments OpenSees donne les écarts suivants pour la flèche au centre :

- ShellMITC4 : +3 % (cisaillement transverse Mindlin, maillage 10 × 10) ;
- ShellDKGT : −0,6 %.

## 10. Limites

- **Calcul :** il est statique linéaire. Le moteur actuel ne gère ni second ordre, ni fissuration, ni analyse modale ou dynamique des coques.
- **Formulations :** seule la coque est calculée. La plaque seule et la membrane seule sont refusées.
- **Liaisons :** aucune excentricité ni liaison rigide n'est générée. Les appuis sont seulement ponctuels (nœuds TSA) : il n'existe pas encore d'appui linéaire sur un bord de surface.
- **Charges :** il n'y a ni charge sur une zone partielle, ni charge variable, ni charge thermique sur les surfaces.
- **Vérifications réglementaires :** les dalles et voiles ne font l'objet d'aucune vérification (ferraillage, poinçonnement, cisaillement des voiles).
