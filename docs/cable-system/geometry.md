# MODÉLISATION GÉOMÉTRIQUE & 3D DES CÂBLES

Le module intègre trois niveaux de modélisation géométrique :
1. **Élément 1D structural** pour les calculs aux éléments finis.
2. **Courbe 3D analytique continue** pour le tracé, l'échantillonnage et les reports de charge.
3. **Solide 3D B-Rep OpenCASCADE volumique réel** pour le rendu cinématique, le contrôle des collisions et la détection d'interférences avec le tablier et les pylônes.

---

## 1. Profils Géométriques Disponibles

### A. Câble Droit (Straight Chord)
- Trajectoire rectiligne directe entre le nœud initial $A(x_A, y_A, z_A)$ et le nœud terminal $B(x_B, y_B, z_B)$.
- Longueur de corde :
  $$L_0 = \|\mathbf{r}_B - \mathbf{r}_A\| = \sqrt{(x_B - x_A)^2 + (y_B - y_A)^2 + (z_B - z_A)^2}$$
- Adapté aux haubans tendus, barres de suspension courtes, suspentes verticales et tirants d'ancrage.

### B. Profil Parabolique (Parabolic Sag Profile)
- Approximation canonique de la suspension sous charge uniformément répartie le long de la corde horizontale :
  $$z(s) = z_A + \frac{s}{L_h}(z_B - z_A) - 4 f \frac{s}{L_h}\left(1 - \frac{s}{L_h}\right)$$
  avec $f$ la flèche au centre et $L_h$ la projection horizontale de la portée.
- Longueur curviligne approchée :
  $$L_{arc} \approx L_0 \left[1 + \frac{8}{3}\left(\frac{f}{L_0}\right)^2 - \frac{32}{5}\left(\frac{f}{L_0}\right)^4\right]$$

### C. Caténaire Rigoureuse (Catenary Profile)
- Équation mécanique d'équilibre sous poids propre uniforme le long de l'arc curviligne :
  $$y(x) = c \cdot \left(\cosh\left(\frac{x - x_0}{c}\right) - 1\right) + y_0$$
  avec :
  - $w$ : poids linéique du câble ($\text{N/m}$)
  - $H$ : composante horizontale de la tension ($\text{N}$)
  - $c = \frac{H}{w}$ : paramètre de la chaînette ($\text{m}$)
- Longueur exacte de l'arc :
  $$s(x) = c \cdot \sinh\left(\frac{x - x_0}{c}\right)$$
- Tension tangentielle à l'abscisse $x$ :
  $$T(x) = \sqrt{H^2 + (w \cdot s(x))^2} = H \cdot \cosh\left(\frac{x - x_0}{c}\right)$$

---

## 2. Génération Solide 3D OpenCASCADE (B-Rep)

La classe `TSA::Geometry::CableGeometry3D` transforme les éléments filaires en solides 3D réels :
- **Solide Cylindrique Droit** :
  `BRepPrimAPI_MakeCylinder` orienté selon l'axe normé $\mathbf{u}_{AB} = (\mathbf{r}_B - \mathbf{r}_A)/L_0$.
- **Balayage B-Rep sur Courbe Arbitraire (Pipe)** :
  1. Échantillonnage de $N$ points spatiaux le long de la caténaire ou parabole.
  2. Construction d'une spline continue $C^2$ via `GeomAPI_Interpolate`.
  3. Construction du profil circulaire perpendiculaire au vecteur tangent initial.
  4. Balayage solide via `BRepOffsetAPI_MakePipe`.
- **Têtes et Culots d'Ancrage** :
  Cylindres ou cônes métalliques de sur-épaisseur centrés sur le nœud et dirigés vers l'intérieur de la travée (`createAnchorSocket`).
