# Références BIM & Interopérabilité IFC (buildingSMART)

## 1. Vue d'Ensemble & Cadre Normatif

La couche BIM de **TSA (Tsaraloha Structural Analysis)** repose sur les standards ouverts d'interopérabilité développés par **buildingSMART International** et standardisés par l'ISO.

| Standard / Spécification | Organisme | Référence Normative | Statut | Usage dans TSA |
| :--- | :--- | :--- | :--- | :--- |
| **IFC 4.3 ADD2** | buildingSMART / ISO | **ISO 16739-1:2024** | Norme internationale en vigueur | Modélisation des infrastructures (ponts, routes) et bâtiments |
| **IFC 4 ADD2 TC1** | buildingSMART / ISO | **ISO 16739-1:2018** | Norme internationale active | Standard cible de l'export/import architectural et structurel |
| **IFC 2x3 TC1** | buildingSMART / ISO | **ISO/PAS 16739:2005** | Historique maintenu (compatibilité) | Importation de maquettes legacy et compatibilité étendue |
| **IDS 1.0 (Information Delivery Specification)** | buildingSMART | Spécification officielle v1.0 (2022) | Standard ouvert | Validation contractuelle des exigences de propriétés (`PropertySet`) |
| **BCF 2.1 / 3.0 (BIM Collaboration Format)** | buildingSMART | BCF-API / XML standard | Standard d'échange de sujets | Traçabilité des commentaires, clashs et demandes de modifications |

---

## 2. Domaine d'Analyse Structurelle IFC (Structural Analysis Domain)

Dans les schémas IFC4 et IFC4.3, l'analyse par éléments finis et le modèle filaire/surfacique sont formalisés dans le **Structural Analysis Domain** :

```text
                                 IfcProduct
                                     │
                 ┌───────────────────┴───────────────────┐
                 ↓                                       ↓
        IfcStructuralItem                      IfcStructuralActivity
                 │                                       │
     ┌───────────┴───────────┐               ┌───────────┴───────────┐
     ↓                       ↓               ↓                       ↓
IfcStructuralMember    IfcStructural      IfcStructuralAction   IfcStructuralReaction
     │                  Connection           (Charges)              (Efforts)
 ┌───┴───┐               ┌───┴───┐
 ↓       ↓               ↓       ↓
Curve  Surface         Point   Curve/Surface
Member Member        Connection Connection
```

### Entités Clés & Mapping TSA

1. **`IfcStructuralAnalysisModel`** :
   - Conteneur global du modèle de calcul (`IfcSystem`).
   - Regroupe l'orientation du repère global ($X, Y, Z$), les cas de charges (`IfcStructuralLoadGroup`), les combinaisons et les éléments structuraux idéalisés.
   - *Correspondance TSA :* [`TSA::Model::Model`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Model.h) et [`TSA::Analysis::CalculationSnapshot`](file:///d:/Tsaraloha%20Platforme/TSA/src/Analysis/CalculationSnapshot.h).

2. **`IfcStructuralCurveMember`** :
   - Idéalisation 1D d'une poutre, d'un poteau ou d'une barre de treillis.
   - Attribut `PredefinedType` : `RIGID_JOINED_MEMBER` (poutre/poteau fléchi), `PIN_JOINED_MEMBER` (treillis traction/compression), `CABLE` (câble traction pure), `TENSION_MEMBER`.
   - *Correspondance TSA :* [`TSA::Model::Beam`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Beam.h), [`TSA::Model::Column`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Column.h), [`TSA::Model::TrussMember`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/TrussMember.h), [`TSA::Model::Cable`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Cable/Cable.h).

3. **`IfcStructuralSurfaceMember`** :
   - Idéalisation 2D d'une dalle, d'un radier ou d'un voile.
   - Attribut `PredefinedType` : `BENDING_ELEMENT` (plaque en flexion pure), `MEMBRANE_ELEMENT` (membrane plancher), `SHELL` (coque 3D combinant membrane + flexion).
   - *Correspondance TSA :* [`TSA::Model::Slab`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Slab.h), [`TSA::Model::Wall`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Wall.h).

4. **`IfcStructuralPointConnection` & `IfcBoundaryNodeCondition`** :
   - Nœud de calcul reliant les barres ou représentant un appui avec conditions aux limites.
   - Raideurs élastiques ou blocages par degré de liberté :
     - `TranslationalStiffnessX, Y, Z` : $\infty$ (bloqué), $0$ (libre), ou valeur numérique en $kN/m$.
     - `RotationalStiffnessX, Y, Z` : $\infty$ (encastrement), $0$ (rotule), ou valeur numérique en $kNm/rad$.
   - *Correspondance TSA :* [`TSA::Model::Node`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Node.h) et [`TSA::Model::SupportDefinition`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/SupportDefinition.h).

5. **`IfcRelConnectsStructuralMember`** :
   - Définit les relâchements d'extrémités (*releases*) entre un membre et une connexion (moments nuls en tête de poteau ou articulation d'une poutre sur corbeau).

6. **Liaison Physique ↔ Analytique (`IfcRelAssignsToProduct`)** :
   - Associe un élément solide architectural (ex. `IfcBeam` volumique) à sa ligne moyenne de calcul `IfcStructuralCurveMember`.

---

## 3. Repères Spatiaux et Géométrie B-Rep

- **Système de Coordonnées Local / Global :**
  - Repère global IFC : Main droite, axe $Z$ vertical ascendant (conforme à TSA).
  - Placement local : `IfcLocalPlacement` relatif au niveau (`IfcBuildingStorey`) ou au bâtiment (`IfcBuilding`).
- **Représentation Géométrique :**
  - Solides par extrusion : `IfcExtrudedAreaSolid` basé sur un profil paramétrique (`IfcIShapeProfileDef`, `IfcRectangleProfileDef`, `IfcCircleProfileDef`).
  - Solides B-Rep facettés : `IfcFacetedBrep` ou `IfcAdvancedBrep`.
  - Solides par balayage : `IfcSectionedSpine` ou `IfcSweptDiskSolid` (utilisé pour le tracé 3D des câbles et haubans).

---

## 4. Liens Officiels & Sources Primaires

* **Portail des spécifications buildingSMART :** [https://standards.buildingsmart.org/](https://standards.buildingsmart.org/)
* **Spécification normative IFC 4.3 :** [https://standards.buildingsmart.org/IFC/RELEASE/IFC4_3/](https://standards.buildingsmart.org/IFC/RELEASE/IFC4_3/)
* **Spécification normative IFC 4 ADD2 TC1 :** [https://standards.buildingsmart.org/IFC/RELEASE/IFC4/ADD2_TC1/HTML/](https://standards.buildingsmart.org/IFC/RELEASE/IFC4/ADD2_TC1/HTML/)
* **buildingSMART Model Support Group (GitHub) :** [https://github.com/buildingSMART/IFC4.3.x-development](https://github.com/buildingSMART/IFC4.3.x-development)
* **Dépôt IfcOpenShell officiel :** [https://github.com/IfcOpenShell/IfcOpenShell](https://github.com/IfcOpenShell/IfcOpenShell)
* **Spécification IDS (Information Delivery Specification) :** [https://github.com/buildingSMART/IDS](https://github.com/buildingSMART/IDS)

---

## 5. Jeux de Données et Fichiers IFC de Référence Présents

Le dépôt TSA intègre déjà un jeu de fichiers IFC de référence officiel buildingSMART dans [`tests/data/ifc/buildingsmart/`](file:///d:/Tsaraloha%20Platforme/TSA/tests/data/ifc/buildingsmart) :
- `ifc2x3_Building-Architecture.ifc` & `ifc2x3_Building-Structural.ifc` : Bâtiment complet à plusieurs étages.
- `ifc2x3_Infra-Bridge.ifc` : Pont d'infrastructure complet (tablier, piles, culées, fondations).
- `ifc4_Building-Architecture.ifc` & `ifc4_Building-Structural.ifc` : Bâtiment IFC4 standardisé.
- `ifc4x3_Building-Architecture.ifc` & `ifc4x3_Building-Structural.ifc` : Modèle conforme à la norme ISO 16739-1:2024.
- `ifc4rv_column-straight-rectangle-tessellation.ifc` & `ifc4rv_wall-with-opening-and-window.ifc` : Cas tests unitaires avec ouvertures et géométries facettées.
