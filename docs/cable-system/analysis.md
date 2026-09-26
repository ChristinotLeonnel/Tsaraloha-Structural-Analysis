# ANALYSE MÉCANIQUE, FORMULATION NON LINÉAIRE & MODULE D'ERNST

Les éléments câbles et tirants se caractérisent par un comportement mécanique fondamentalement non linéaire induit par la non-linéarité géométrique (grands déplacements, effet de flèche sous poids propre) et par la non-linéarité matérielle unilatérale (résistance nulle en compression).

Dans **TSA**, ces aspects sont régis par l'interface [ICableElement](file:///E:/Book/Dev/TSA/src/Model/Cable/CableAnalysisProperties.h#L11-L24) et la structure [CableAnalysisProperties](file:///E:/Book/Dev/TSA/src/Model/Cable/CableAnalysisProperties.h#L26-L65).

---

## 1. Comportement en Traction Seule (*Tension-Only*)

Un câble ne transmet aucun effort de compression ni moment fléchissant significatif.
- **Régime Tendu ($T > 0$)** : L'élément développe une raideur axiale active couplée à la raideur géométrique sous tension.
- **Régime Mou / Décomprimé ($T \le 0$)** :
  L'élément devient inactif (*slack cable*). Sa matrice de raideur tangente s'annule :
  $$\mathbf{K}_{elem} = \mathbf{0}$$
  Le solveur non linéaire de TSA procède à une condensation ou une réattribution d'une raideur résiduelle minimale de stabilisation numérique ($10^{-6} \cdot K_{axiale}$) pour éviter la singularité de la matrice globale.

---

## 2. Effet de Flèche & Formulation du Module d'Ernst ($E_{eq}$)

Sous l'effet combiné du poids propre $w$ ($\text{N/m}$) et de l'effort axial $T$ ($\text{N}$), le câble présente une courbure caténaire/parabolique. Une augmentation de traction tend le câble et résorbe la flèche, créant un allongement apparent supérieur à l'allongement élastique pur.

Pour modéliser fidèlement ce comportement dans un calcul par éléments finis sans sur-discrétiser chaque hauban en dizaines de micro-barres, H. J. Ernst a formulé un **module d'élasticité équivalent sécant** $E_{eq}$ (intégré dans l'**EN 1993-1-11, Annexe A**) :

$$E_{eq} = \frac{E}{1 + \frac{(w \cdot L_{horiz})^2 \cdot E \cdot A_{metal}}{12 \cdot T^3}}$$

Exprimé en fonction de la contrainte normale $\sigma = \frac{T}{A_{metal}}$ et du poids volumique effectif $\gamma = \frac{w}{A_{metal}}$ :
$$E_{eq} = \frac{E}{1 + \frac{\gamma^2 \cdot L_{horiz}^2 \cdot E}{12 \cdot \sigma^3}}$$

### 2.1 Propriétés et Limites du Module d'Ernst
- **Tension Infinie ($\sigma \to \infty$)** : $E_{eq} \to E$ (le câble tend vers une barre rigide rectiligne).
- **Tension Faible ($\sigma \to 0$)** : $E_{eq} \to 0$ (la flèche absorbe l'effort sans raideur apparente).
- **Portée Courte ($L \to 0$)** : $E_{eq} \approx E$ (l'effet de pesanteur devient négligeable).

Dans le code de TSA ([CableAnalysisProperties.h](file:///E:/Book/Dev/TSA/src/Model/Cable/CableAnalysisProperties.h#L52-L65)), cette formulation est implémentée avec gardes numériques pour prévenir toute division par zéro ou instabilité :

```cpp
double equivalentErnstModulus(double E, double weightPerMeter, double chordLength, double area, double tension)
{
    if (tension <= 1.0e-3 || chordLength <= 0.0 || area <= 0.0)
        return 1.0e-3; // Raideur résiduelle pour câble mou

    double wL = weightPerMeter * chordLength;
    double denom = 1.0 + (wL * wL * E * area) / (12.0 * tension * tension * tension);
    return E / denom;
}
```

---

## 3. Matrice de Raideur Tangente Complète

Pour les analyses non linéaires avancées en grands déplacements ($P-\Delta$ et grands angles) :
$$\mathbf{K}_{tangente} = \mathbf{K}_{materiau} + \mathbf{K}_{geometrique} = \frac{E_{eq} \cdot A}{L} \cdot \left[ \mathbf{k}_0 \right] + \frac{T}{L} \cdot \left[ \mathbf{k}_\sigma \right]$$

- $\mathbf{K}_{materiau}$ : Raideur axiale modifiée par le module d'Ernst.
- $\mathbf{K}_{geometrique}$ : Matrice de contrainte initiale dépendant de la tension instantanée $T$, gouvernant la raideur transversale aux nœuds.

---

## 4. Vérification à l'État Limite Ultime (ELU) selon l'EN 1993-1-11

La résistance de calcul $F_{Rd}$ d'un élément tendu vérifie :
$$F_{Ed} \le F_{Rd} = \min\left( \frac{F_{uk}}{1.50 \cdot \gamma_R}, \; \frac{F_{0.1k}}{\gamma_R} \right)$$
où :
- $F_{uk}$ : Charge de rupture caractéristique sur toron/câble nu.
- $\gamma_R$ : Coefficient partiel de sécurité ($\gamma_R = 1.00$ à $1.15$ selon l'assurance qualité et les essais de culottage).
