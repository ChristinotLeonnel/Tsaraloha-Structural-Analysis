# Modélisation Géométrique B-Rep & Maillage Éléments Finis

## 1. Noyau Géométrique CAO — OpenCASCADE Technology (OCCT 8.0.1)

TSA s'appuie sur le moteur géométrique et topologique de classe industrielle **OpenCASCADE Technology (OCCT)** pour la représentation exacte volumique B-Rep (*Boundary Representation*) des solides de structure.

### 1.1 Architecture Topologique & Données B-Rep
OCCT structure les formes géométriques selon une hiérarchie stricte dans le package `TopoDS` :

```text
TopoDS_Compound    (Groupe hétérogène de formes)
      │
TopoDS_CompSolid   (Solides adjacents partageant des faces communes)
      │
TopoDS_Solid       (Volume 3D fermé défini par une ou plusieurs coquilles)
      │
TopoDS_Shell       (Ensemble connexe de faces 3D cousues)
      │
TopoDS_Face        (Surface 2D bornée par un ou plusieurs contours)
      │
TopoDS_Wire        (Contour continu fermé composé d'arêtes)
      │
TopoDS_Edge        (Courbe 1D délimitée par deux sommets)
      │
TopoDS_Vertex      (Point géométrique 3D de coordonnées gp_Pnt)
```

### 1.2 Algorithmes Constructifs dans TSA
1. **Extrusion Prismatique de Profilés :**
   - Construction du contour de la section droite via `BRepBuilderAPI_MakeWire` et `BRepBuilderAPI_MakeFace`.
   - Extrusion linéaire le long du vecteur directeur $\vec{L} = \vec{N}_j - \vec{N}_i$ par `BRepPrimAPI_MakePrism`.
   - Utilisé par [`TSA::Geometry::BeamGeometry`](file:///d:/Tsaraloha%20Platforme/TSA/src/Geometry/BeamGeometry.h).
2. **Surfaces & Dalles Poligonales Épaisses :**
   - Construction de la face polygonale plane à partir des sommets ordonnés.
   - Épaississement normal par prismation ou décalage (`BRepOffsetAPI_MakeThickSolid`).
   - Utilisé par [`TSA::Geometry::SlabGeometry`](file:///d:/Tsaraloha%20Platforme/TSA/src/Geometry/SlabGeometry.h) et [`TSA::Geometry::WallGeometry`](file:///d:/Tsaraloha%20Platforme/TSA/src/Geometry/WallGeometry.h).
3. **Opérations Booléennes Robustes :**
   - Perçages, trémies, réservations et chanfreins via `BRepAlgoAPI_Cut` et `BRepAlgoAPI_Fuse`.

---

## 2. Générateur de Maillage EF — Gmsh 4.15.2 SDK

Pour le passage de la géométrie CAO surfacique continue aux éléments finis discrets, TSA encapsule le moteur de maillage **Gmsh** dans [`src/Mesh/Gmsh/GmshMeshEngine.h`](file:///d:/Tsaraloha%20Platforme/TSA/src/Mesh/Gmsh/GmshMeshEngine.h).

### 2.1 Algorithmes de Maillage Surfacique 2D
* **Delaunay (Algorithme 5) :** Robuste, adapté aux géométries à contours complexes et aux transitions d'épaisseurs.
* **Frontal-Delaunay (Algorithme 6) :** Algorithme par défaut pour les maillages triangulaires réguliers de haute qualité géométrique.
* **Maillage Transfini / Structuré :** Génère un quadrillage régulier parfait en éléments quadrangulaires (Q4) sur les dalles rectangulaires simples sans singularité.
* **BAMG / MeshAdapt :** Raffinement adaptatif basé sur le gradient d'effort ou de contrainte calculé.

### 2.2 Métriques de Qualité des Éléments Finis
Dans [`src/Mesh/MeshQuality.h`](file:///d:/Tsaraloha%20Platforme/TSA/src/Mesh/MeshQuality.h), chaque élément généré est évalué selon :
1. **Ratio d'aspect (Aspect Ratio) :** Ratio entre le rayon du cercle circonscrit et le rayon du cercle inscrit. Idéal = $1.0$ (triangle équilatéral).
2. **Déterminant Jacobien Normalisé (SICN) :**
   $$SICN = \frac{\min(\det J)}{\max(\det J)} \in [-1, 1]$$
   Un $SICN \ge 0.5$ garantit l'absence d'éléments distordus ou dégénérés.
3. **Angle intérieur minimal et maximal :** Surveillance des angles aigus ($< 20^\circ$) et obtus ($> 140^\circ$) provoquant des erreurs numériques d'interpolation.

---

## 3. Visualisation Haute Performance — MeshVS

L'affichage interactif des millions de facettes de maillage ne surcharge pas le graphe de scène standard d'OpenCASCADE : il exploite le composant optimisé **MeshVS (Mesh Visualization Service)** d'OCCT :
- Source de données customisée [`TSA::Viewer::MeshVSDataSource`](file:///d:/Tsaraloha%20Platforme/TSA/src/Viewer/MeshVSDataSource.h).
- Cartographie instantanée des champs scalaires de résultats (déplacements, contraintes de von Mises) sans réallocation de mémoire GPU.

---

## 4. Documentation & Sources Officielles

1. **OpenCASCADE Technology** — *Modeling Data & Algorithms User Guide*, Version 8.0.1, [https://dev.opencascade.org/doc/overview/html/](https://dev.opencascade.org/doc/overview/html/).
2. **Geuzaine, C., Remacle, J.-F.** — *Gmsh: A 3-D finite element mesh generator with built-in pre- and post-processing facilities*, International Journal for Numerical Methods in Engineering, [https://gmsh.info/](https://gmsh.info/).
