# SYSTÈMES SUSPENDUS & CÂBLES PORTEURS

Le module Cable & Tension System de **TSA** intègre un générateur et un moteur analytique de ponts suspendus complets via la classe [SuspensionBridge](file:///E:/Book/Dev/TSA/src/Model/Cable/SuspensionSystem.h#L18-L80).

---

## 1. Principes Fondamentaux de Modélisation

Un système de pont suspendu est constitué de trois sous-systèmes interdépendants :
1. **Les Câbles Porteurs Principaux (Main Suspension Cables)** : Profils courbes franchissant la travée principale entre les têtes de pylônes.
2. **Les Câbles de Retenue (Backstays)** : Câbles ancrés aux massifs gravitaires d'extrémité et fixés au sommet des pylônes.
3. **Le Réseau de Suspentes (Hangers)** : Éléments verticaux ou inclinés reliant le câble porteur aux poutres de rive ou au caisson du tablier.

---

## 2. Formulations Géométriques et Analytiques

### 2.1 Équation Parabolique (Approximation sous charge uniforme horizontale)
Pour un câble de portée $L$ soumis à une charge uniforme par unité de longueur projetée horizontale $q$ :
$$y(x) = \frac{4 \cdot f}{L^2} \cdot x \cdot (L - x)$$
où :
- $f$ : Flèche maximale au milieu de travée ($x = L/2$).
- L'effort horizontal constant est : $H = \frac{q \cdot L^2}{8 \cdot f}$.
- La tension maximale aux appuis est : $T_{max} = \sqrt{H^2 + \left(\frac{q \cdot L}{2}\right)^2}$.

### 2.2 Équation Caténaire Exacte (Sous poids propre curviligne)
Sous poids propre surfacique/linéique réel $w$ ($\text{N/m}$) le long de l'arc :
$$y(x) = c \cdot \left(\cosh\left(\frac{x - x_0}{c}\right) - 1\right) + y_0$$
avec le paramètre de caténaire caractéristique :
$$c = \frac{H}{w}$$
La tension le long de la caténaire vérifie rigoureusement :
$$T(s) = H \cdot \frac{ds}{dx} = w \cdot (y(x) + c)$$

La longueur d'arc curviligne exacte d'une caténaire symétrique de portée $L$ et flèche $f$ est :
$$S = 2 \cdot c \cdot \sinh\left(\frac{L}{2 \cdot c}\right) \approx L \cdot \left(1 + \frac{8}{3}\left(\frac{f}{L}\right)^2 - \frac{32}{5}\left(\frac{f}{L}\right)^4\right)$$

---

## 3. Génération Automatisée dans TSA

La classe `SuspensionBridge` permet de générer un pont suspendu complet à partir de quelques paramètres géométriques :

```cpp
#include "Model/Cable/SuspensionSystem.h"

// Création d'un pont suspendu de portée 200 m et flèche 20 m
TSA::Model::SuspensionBridge bridge("Pont Suspendu Principal", 200.0, 20.0);

// Définition des coordonnées des têtes de pylônes et massifs d'ancrage
gp_Pnt pylonLeft(0.0, 0.0, 35.0);
gp_Pnt pylonRight(200.0, 0.0, 35.0);
gp_Pnt anchorLeft(-60.0, 0.0, 0.0);
gp_Pnt anchorRight(260.0, 0.0, 0.0);

// Génération automatique des câbles porteurs et suspentes dans le modèle
int numHangers = 19; // Espacement de 10 m
bridge.generateHangers(model, pylonLeft, pylonRight, numHangers, deckElevation);
```

### 3.1 Algorithme de Génération des Suspentes
1. Discrétisation régulière de l'axe longitudinal du tablier à des abscisses $x_k = \frac{k}{N+1} \cdot L$.
2. Calcul analytique de la cote $z(x_k)$ sur la courbe porteuse (mode caténaire ou parabolique).
3. Création automatique du nœud supérieur $P_{top}(x_k, y_k, z_k)$ sur le câble porteur.
4. Création automatique du nœud inférieur $P_{deck}(x_k, y_k, z_{deck})$ au niveau du tablier.
5. Instanciation d'un élément [Cable](file:///E:/Book/Dev/TSA/src/Model/Cable/Cable.h) de type `CableType::Hanger` reliant ces deux nœuds.
6. Attribution d'une section et d'un matériau adaptés au catalogue (ex: câble toron clos ou barre haute résistance).

---

## 4. Affichage et Rendu 3D OpenCASCADE

- Les câbles porteurs sont modélisés comme des solides B-Rep balayés (`BRepOffsetAPI_MakePipe`) sur une courbe B-Spline `GeomAPI_Interpolate` passant par les nœuds de fixation des suspentes.
- Les suspentes sont représentées par des cylindres solides exacts (`BRepPrimAPI_MakeCylinder`) raccordés à leurs têtes d'ancrage (culots coniques ou fourches filetées).
