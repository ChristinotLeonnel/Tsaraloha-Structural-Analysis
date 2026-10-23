# Méthode des Éléments Finis & Formulation Mathématique (FEM Theory)

## 1. Principes Fondamentaux de la Méthode des Déplacements

Le comportement statique linéaire d'un système discret à éléments finis est régi par l'équation matricielle d'équilibre global :

$$[K] \{U\} = \{F_{ext}\} - \{F_0\}$$

où :
* $[K]$ est la matrice de rigidité globale assemblée (symétrique, définie positive après application des conditions aux limites).
* $\{U\}$ est le vecteur des déplacements et rotations nodales aux degrés de liberté (DDL) libres.
* $\{F_{ext}\}$ est le vecteur des charges nodales extérieures directement appliquées.
* $\{F_0\}$ est le vecteur des forces nodales équivalentes issues des charges réparties le long des éléments (actions de fixation parfaite).

---

## 2. Éléments Poutres & Poteaux 3D (Frame Elements)

Chaque élément linéaire 1D reliant deux nœuds $i$ et $j$ dispose de **6 DDL par nœud** dans son repère local :
$$\{u_l\} = [u_{xi}, u_{yi}, u_{zi}, \theta_{xi}, \theta_{yi}, \theta_{zi}, u_{xj}, u_{yj}, u_{zj}, \theta_{xj}, \theta_{yj}, \theta_{zj}]^T$$

### 2.1 Formulation Euler-Bernoulli (Sans déformation d'effort tranchant)
Valable pour les éléments élancés ($L/h > 10$). Les termes de la matrice locale $[k_l]_{12 \times 12}$ sont :

* **Traction / Compression (Axe $x$) :**
  $$k_{1,1} = k_{7,7} = \frac{EA}{L}, \quad k_{1,7} = k_{7,1} = -\frac{EA}{L}$$

* **Torsion pure de Saint-Venant (Axe $x$) :**
  $$k_{4,4} = k_{10,10} = \frac{GJ}{L}, \quad k_{4,10} = k_{10,4} = -\frac{GJ}{L}$$

* **Flexion dans le plan local $xy$ (Inertie $I_z$) :**
  $$k_{2,2} = \frac{12EI_z}{L^3}, \quad k_{2,6} = \frac{6EI_z}{L^2}, \quad k_{6,6} = \frac{4EI_z}{L}, \quad k_{6,12} = \frac{2EI_z}{L}$$

* **Flexion dans le plan local $xz$ (Inertie $I_y$) :**
  $$k_{3,3} = \frac{12EI_y}{L^3}, \quad k_{3,5} = -\frac{6EI_y}{L^2}, \quad k_{5,5} = \frac{4EI_y}{L}, \quad k_{5,11} = \frac{2EI_y}{L}$$

### 2.2 Formulation Timoshenko (Avec déformation d'effort tranchant)
Indispensable pour les barres courtes, les poutres de transfert et les profilés à âme mince.
Introduction des coefficients sans dimension d'influence de l'effort tranchant $\Phi_y$ et $\Phi_z$ :

$$\Phi_y = \frac{12 E I_z}{G A_{sy} L^2}, \quad \Phi_z = \frac{12 E I_y}{G A_{sz} L^2}$$

où $A_{sy} = \kappa_y A$ et $A_{sz} = \kappa_z A$ sont les aires réduites de cisaillement ($\kappa \approx 5/6$ pour un rectangle, $\kappa \approx A_{âme}/A$ pour un profilé en I).

Les termes de flexion deviennent :
$$k_{2,2} = \frac{12 E I_z}{L^3 (1 + \Phi_y)}, \quad k_{2,6} = \frac{6 E I_z}{L^2 (1 + \Phi_y)}, \quad k_{6,6} = \frac{(4 + \Phi_y) E I_z}{L (1 + \Phi_y)}, \quad k_{6,12} = \frac{(2 - \Phi_y) E I_z}{L (1 + \Phi_y)}$$

---

## 3. Torsion Non-Uniforme & Gauchissement (Vlasov — 7 DDL par nœud)

Pour les profilés métalliques ouverts (IPE, HEA, UPN, cornières), la torsion s'accompagne d'un **gauchissement empêché** (*warping*) induisant des contraintes normales longitudinales $\sigma_w$.
On ajoute un 7ᵉ DDL par nœud : le taux de gauchissement $\theta'_{x} = \omega$ associé au **bimoment $B$** (en $kN \cdot m^2$) :

$$T(x) = T_{sv} + T_{w} = G J \frac{d\theta_x}{dx} - E I_w \frac{d^3\theta_x}{dx^3}$$

où :
* $J$ est la constante de torsion de Saint-Venant ($m^4$).
* $I_w$ (ou $C_w$) est le module d'inertie de gauchissement sectoriel ($m^6$).
* $B(x) = -E I_w \frac{d^2\theta_x}{dx^2}$ est le bimoment.

---

## 4. Éléments Plaques et Coques (Slabs & Shells)

La modélisation des dalles et voiles dans TSA utilise des éléments finis surfaciques 2D à 3 ou 4 nœuds combinant deux états :

1. **Comportement de Membrane (Efforts dans le plan $N_x, N_y, N_{xy}$) :**
   - Élément CST (Constant Strain Triangle) ou Q4 bilinéaire isoparamétrique.
   - Ajout d'un degré de liberté rotationnel fictif normal au plan (*drilling DOF*) régularisé pour éviter les singularités d'assemblage 3D.
2. **Comportement de Flexion de Plaque (Moments $M_x, M_y, M_{xy}$ et tranchants $V_x, V_y$) :**
   - **Plaques minces (Kirchhoff) :** Élément **DKT** (*Discrete Kirchhoff Triangle*, Batoz et al.). La condition de Kirchhoff $\gamma_{xz} = \gamma_{yz} = 0$ est imposée discrètement aux points médians des côtés.
   - **Plaques épaisses (Mindlin-Reissner) :** Élément **MITC4** ou **DKQ** évitant le verrouillage en cisaillement (*shear locking*).

---

## 5. Matrice de Transformation Géométrique ($3D$)

Le passage entre le repère local de l'élément $\{u_l\}$ et le repère global $\{U_g\}$ s'effectue via la matrice orthogonale de direction $[T]_{12 \times 12}$ :

$$[k_g] = [T]^T [k_l] [T]$$

où $[T]$ est composée de blocs diagonaux $[\Lambda]_{3 \times 3}$ :
$$[\Lambda] = \begin{bmatrix}
l_x & m_x & n_x \\
l_y & m_y & n_y \\
l_z & m_z & n_z
\end{bmatrix}$$

où :
* $\vec{x} = \frac{\vec{N}_j - \vec{N}_i}{L}$ définit l'axe longitudinal de la barre.
* $\vec{z}$ et $\vec{y}$ sont construits à l'aide d'un vecteur vertical de référence (ou de l'angle de roulis *roll angle* $\beta$).

---

## 6. Références Théoriques Majeures

1. **Bathe, K.J.** (2014) — *Finite Element Procedures*, 2nd Edition, Prentice Hall.
2. **Zienkiewicz, O.C., Taylor, R.L., Zhu, J.Z.** (2013) — *The Finite Element Method: Its Basis and Fundamentals*, 7th Edition, Butterworth-Heinemann.
3. **Batoz, J.L., Dhatt, G.** (1990) — *Modélisation des structures par éléments finis : Poutres et plaques (Vol. 2) & Coques (Vol. 3)*, Éditions Hermès.
4. **Vlasov, V.Z.** (1961) — *Thin-Walled Elastic Beams*, Israel Program for Scientific Translations, Jerusalem.
5. **OpenSees Theory Manual** — Pacific Earthquake Engineering Research Center (PEER), UC Berkeley.
