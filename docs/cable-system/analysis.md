# ANALYSE MÉCANIQUE DES CÂBLES & MODULE D'ERNST

TSA prend actuellement en charge l'analyse structurale linéaire. L'analyse non linéaire n'est pas disponible dans cette
version (ADR-025). Ce document décrit ce que le calcul fait réellement d'un câble, et ce qu'il ne fait pas.

Les données d'un câble sont portées par l'interface `ICableElement` et la structure `CableAnalysisProperties`
(`src/Model/Cable/CableAnalysisProperties.h`). L'élément OpenSees est écrit par `OpenSeesAnalysisBuilder::buildElements`
(`src/Analysis/OpenSeesAnalysisBuilder.cpp`).

---

## 1. Modèle de calcul d'un câble (OpenSees)

| Point | Ce que fait TSA |
|---|---|
| Élément | `corotTruss` : barre articulée, effort normal seul |
| Matériau | `uniaxialMaterial Elastic` (E) |
| Précontrainte | `uniaxialMaterial InitStrain`, ε₀ = T₀ / (E·A) |
| Résolution | un seul pas : `algorithm Linear`, `integrator LoadControl 1.0`, `analyze 1`, à la géométrie initiale, sans itération |
| Compression | **non relâchée** : un câble comprimé reste actif et son effort normal négatif est rendu tel quel (BUG-151) |

### 1.1 Raideur de la précontrainte

La matrice résolue est la tangente initiale de `corotTruss`. Elle contient la raideur géométrique due à la précontrainte
(raideur transversale T₀/L). C'est un calcul linéarisé autour de l'état précontraint : il permet de calculer des
câbles dont les nœuds n'ont pas d'autre raideur transversale.

Essai du 31/10/2026 (OpenSees 3.8.0) : corde de 2 × 5 m, T₀ = 20 kN, nœud central chargé de 1 kN transversalement.

| Élément | Déplacement transversal | Commentaire |
|---|---|---|
| `corotTruss` + `InitStrain` | −0,125 m | égal à la raideur de corde 2·T₀/a = 8 kN/m |
| `truss` + `InitStrain` | aucun : matrice singulière | pas de raideur transversale sans la précontrainte |

### 1.2 Effort recalculé sur la géométrie déformée

`corotTruss` recalcule l'allongement, donc l'effort normal, sur la géométrie déformée. Le pas étant unique, ces efforts
ne sont pas rééquilibrés : il reste un résidu d'équilibre qui croît comme (u/L)².

- Sur l'essai ci-dessus (u/a = 2,5 %), N = 26,25 kN au lieu de 20 kN et la somme des réactions verticales vaut 1,31 kN
  pour 1 kN appliqué.
- Sur un pont à haubans réaliste, le résidu mesuré est d'environ 2 × 10⁻⁵ (`docs/AUDIT_CALCULS.md` § 4.2).

Le résultat d'un câble n'est donc fiable que pour de petits déplacements devant sa longueur. Ce modèle est conservé tel
quel, car il porte des résultats validés. Le remplacer (par exemple par `truss`) changerait ces résultats et rendrait
singuliers les modèles dont les câbles n'ont pas d'autre raideur transversale : c'est une décision à prendre.

### 1.3 Ce qui n'est pas calculé

- **Traction seule** : `CableAnalysisProperties::tensionOnly` est une donnée descriptive (bibliothèques, fichiers, TSA3D).
  Aucun moteur ne la lit, et aucun câble détendu n'est retiré du calcul.
- **Grands déplacements, itérations d'équilibre, réglage itératif des tensions** : relèvent de l'analyse non linéaire,
  non disponible.
- **Flèche sous poids propre (module d'Ernst)** : la formule est implémentée dans le modèle (§ 2), mais le calcul ne
  l'utilise pas : le câble est calculé avec son module E.

La note de calcul rappelle ces limites et contrôle la tension de chaque câble a posteriori (N > 0).

---

## 2. Effet de Flèche & Formulation du Module d'Ernst ($E_{eq}$)

Sous l'effet combiné du poids propre $w$ ($\text{N/m}$) et de l'effort axial $T$ ($\text{N}$), un câble réel présente une
courbure caténaire ou parabolique. Une augmentation de traction résorbe la flèche et crée un allongement apparent
supplémentaire.

H. J. Ernst a formulé un **module d'élasticité équivalent sécant** $E_{eq}$ qui représente cet effet sans discrétiser le
câble en nombreuses barres :

$$E_{eq} = \frac{E}{1 + \frac{(w \cdot L_{horiz})^2 \cdot E \cdot A_{metal}}{12 \cdot T^3}}$$

Exprimé en fonction de la contrainte normale $\sigma = \frac{T}{A_{metal}}$ et du poids volumique effectif $\gamma = \frac{w}{A_{metal}}$ :
$$E_{eq} = \frac{E}{1 + \frac{\gamma^2 \cdot L_{horiz}^2 \cdot E}{12 \cdot \sigma^3}}$$

### 2.1 Propriétés et Limites du Module d'Ernst
- **Tension infinie ($\sigma \to \infty$)** : $E_{eq} \to E$ (le câble tend vers une barre rectiligne).
- **Tension faible ($\sigma \to 0$)** : $E_{eq} \to 0$ (la flèche absorbe l'effort sans raideur apparente).
- **Portée courte ($L \to 0$)** : $E_{eq} \approx E$ (l'effet de pesanteur devient négligeable).

Implémentation : `CableAnalysisProperties::calculateErnstEquivalentModulus` et `Cable::equivalentErnstModulus`, avec des
gardes numériques (tension, portée, module ou section nuls : E ; facteur supérieur à 10⁶ : E / 10⁶). Testée par la
suite `cables`. **Non utilisée par le calcul** (§ 1.3).

---

## 3. Vérification à l'État Limite Ultime (ELU) selon l'EN 1993-1-11

La résistance de calcul $F_{Rd}$ d'un élément tendu vérifie :
$$F_{Ed} \le F_{Rd} = \min\left( \frac{F_{uk}}{1.50 \cdot \gamma_R}, \; \frac{F_{0.1k}}{\gamma_R} \right)$$
où :
- $F_{uk}$ : Charge de rupture caractéristique sur toron/câble nu.
- $\gamma_R$ : Coefficient partiel de sécurité ($\gamma_R = 1.00$ à $1.15$ selon l'assurance qualité et les essais de culottage).

Cette vérification réglementaire est distincte de l'analyse : elle n'est pas validée du seul fait que l'analyse est
linéaire.
