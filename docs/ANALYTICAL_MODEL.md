# Modèle physique et modèle analytique

> Statut : implémentation partielle, conçue selon les principes d'IFC 4.3 (ISO 16739-1:2024).
> Aucune conformité n'est revendiquée. Voir aussi `docs/BIM_ARCHITECTURE.md`.

## 1. Deux modèles, une source de vérité

| Couche | Contenu | Code |
| :--- | :--- | :--- |
| Analytique | Nœuds, barres (poutres, poteaux, treillis, câbles), dalles, voiles, fondations, appuis, relâchements, charges | `src/Model` (`TSA::Model::Model`) |
| Physique / BIM | Produits porteurs (IfcBeam, IfcColumn, IfcMember, IfcSlab, IfcWall, IfcFooting, IfcPile), structure spatiale, GlobalId, Psets, classifications | `src/BIM/Core` (`TSA::BIM::BimModel`) |

Les éléments TSA existants **sont** le modèle analytique : le solveur (OpenSees, Custom2D) n'a pas
été modifié. La couche BIM est additive et vit dans le `Model` (`Model::bim()`), donc dans les
mêmes snapshots Annuler / Rétablir et dans le même fichier `.tsa` (chunk `BIMM`).

## 2. PhysicalToAnalyticalMap (1 → N)

Chaque `PhysicalElement` porte la liste **ordonnée** (sens de l'axe) de ses éléments analytiques
(`AnalyticalRef { ElementKind, id }`). Index inverse : `BimModel::physicalOf(ref)`.

| Opération | Effet sur le mapping |
| :--- | :--- |
| Création d'un élément | Un produit 1:1 (catégorie / PredefinedType par défaut : `defaultCategory`) |
| Division (`splitBeam`, `splitColumn`, `splitBarAt`, nettoyage, Intersecter) | Les tronçons rejoignent le produit de l'original, juste après lui (`attachSplit`) |
| Copie / répétition (`copyElements`, `copyAndRotateElements`) | Un nouveau produit par produit source, mêmes métadonnées, nouveaux identifiants (`registerCopies`) |
| Suppression | Référence retirée ; produit supprimé s'il ne contient plus rien |
| Modification (section, nœuds, matériau…) | Aucune : l'identité ne change jamais |
| Regrouper / Séparer | `BimModel::group`, `BimModel::ungroup` |

Limite connue : le collage du presse-papiers structurel (`StructuralClipboard`) ne transmet pas
encore les métadonnées BIM ; les éléments collés reçoivent un produit 1:1.

## 3. Identifiants

- `PhysicalElement::id` : identifiant interne TSA, croissant, jamais réutilisé.
- `PhysicalElement::globalId` : `IfcGloballyUniqueId` (22 caractères), distinct de l'id interne.
- Objets analytiques : GlobalId propre par nœud / barre / surface (`analyticalGlobalId`), utilisé
  pour `IfcStructuralPointConnection` / `IfcStructuralCurveMember` à l'export.
- Stabilité : conservés par Annuler / Rétablir (le snapshot synchronise la couche BIM avant
  capture), par l'enregistrement et le rechargement, et par toute modification.

## 4. Synchronisation

`Model::bim()` synchronise paresseusement (signature : révision + tailles + compteurs + niveaux) :
produits manquants créés, références orphelines retirées, GlobalId manquants attribués, un étage
par niveau du `LevelManager`. L'opération est idempotente et ne régénère jamais un GlobalId valide.

## 5. Étage d'un produit

`BimModel::resolvedStorey` : étage imposé (`storeyLevelId`), sinon niveau du nœud le plus bas
(`Node::levelId`), sinon niveau de cote ≤ z la plus haute. Un repère de grille (« B-3 ») est une
information BIM seulement : il ne remplace jamais les coordonnées.

## 6. Tests

Suite `bim` (tests 170–176) : création, modification, suppression + Annuler, mapping 1:N
(division, copie, regroupement), étages, persistance `.tsa`, robustesse.
