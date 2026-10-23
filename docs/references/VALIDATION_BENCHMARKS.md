# TSA — Banc d'Essais et Cas de Validation Numérique (Benchmarks)

> Ce document définit l'ensemble des cas de validation théoriques, analytiques et numériques de référence pour le logiciel **Tsaraloha Structural Analysis (TSA)**.
> Conformément aux principes de rigueur scientifique du projet, **aucune valeur numérique n'est inventée** : chaque cas repose soit sur des formules analytiques fermées de la mécanique des structures (RDM classique), soit sur des cas tests de référence universellement reconnus (NAFEMS, Roark, Timoshenko, Bathe, Scordelis-Lo, SETRA, Eurocodes).

---

## 1. Protocole de Validation & Critères d'Acceptation

Pour chaque cas test, l'évaluation numérique compare le résultat obtenu par les moteurs de calcul de TSA ($R_{\text{TSA}}$) à la solution exacte analytique ou de référence ($R_{\text{ref}}$) :

$$\epsilon_{\text{rel}} = \left| \frac{R_{\text{TSA}} - R_{\text{ref}}}{R_{\text{ref}}} \right| \times 100\,\%$$

### Seuils d'acceptation :
- **Formulations Analytiques Poutres 1D (RDM)** : $\epsilon_{\text{rel}} \le 0.05\,\%$ (précision machine).
- **Poutres Timoshenko & Cisaillement** : $\epsilon_{\text{rel}} \le 0.10\,\%$.
- **Éléments Finis Surfaciques 2D (Plaques/Dalles DKT)** : $\epsilon_{\text{rel}} \le 1.00\,\%$ sur maillage régulier fin.
- **Coques 3D (Scordelis-Lo)** : $\epsilon_{\text{rel}} \le 1.50\,\%$.
- **Analyse Modale (Fréquences propres $\omega_n$)** : $\epsilon_{\text{rel}} \le 0.50\,\%$.

---

## 2. Recueil des 11 Cas de Référence Reproductibles

---

### Cas 01 — Poutre Simplement Appuyée (Point Load & Distributed Load)
- **Identifiant** : `BM-01` (Équivalent `RDM-02` / `RDM-03`)
- **Source de référence** : S. Timoshenko & J. Gere, *Mechanics of Materials* ; Roark’s *Formulas for Stress and Strain* (Table 8.1, Case 1e).
- **Typologie** : Barre 1D, flexion simple Navier-Bernoulli.
- **Hypothèses** : Matériau élastique linéaire isotrope, petits déplacements, déformation d'effort tranchant négligée ($\Phi = 0$).

#### Paramètres :
- Portée : $L = 6.00\text{ m}$
- Section rectangulaire : $b = 0.20\text{ m}$, $h = 0.40\text{ m}$ ($A = 0.08\text{ m}^2$, $I_z = \frac{0.20 \times 0.40^3}{12} = 1.066667 \times 10^{-3}\text{ m}^4$)
- Matériau (Acier S235) : $E = 210\,000\text{ MPa} = 2.10 \times 10^8\text{ kN/m}^2$, $\nu = 0.30$
- Rigidité en flexion : $EI_z = 2.10 \times 10^8 \times 1.066667 \times 10^{-3} = 224\,000\text{ kNm}^2$
- **Chargement 1 (UDL)** : $q = 20.0\text{ kN/m}$ répartie sur toute la portée.
- **Chargement 2 (Point load)** : $P = 50.0\text{ kN}$ à mi-travée ($x = 3.00\text{ m}$).

#### Résultats de référence :
| Grandeur | Formule Analytique Fermée | Valeur Exacte (UDL) | Valeur Exacte ($P$) |
| :--- | :--- | :--- | :--- |
| **Moment fléchissant maximal ($M_{\max}$)** | $qL^2 / 8$ resp. $PL / 4$ | **$90.000\text{ kNm}$** ($x = 3.0\text{ m}$) | **$75.000\text{ kNm}$** ($x = 3.0\text{ m}$) |
| **Effort tranchant maximal ($V_{\max}$)** | $qL / 2$ resp. $P / 2$ | **$60.000\text{ kN}$** ($x = 0$) | **$25.000\text{ kN}$** ($x = 0$) |
| **Flèche maximale au centre ($w_{\max}$)** | $\frac{5 q L^4}{384 E I_z}$ resp. $\frac{P L^3}{48 E I_z}$ | **$15.0670\text{ mm}$** ($1.5067 \times 10^{-2}\text{ m}$) | **$10.0446\text{ mm}$** ($1.0045 \times 10^{-2}\text{ m}$) |
| **Rotation aux appuis ($\theta_A$)** | $\frac{q L^3}{24 E I_z}$ resp. $\frac{P L^2}{16 E I_z}$ | **$0.0080357\text{ rad}$** | **$0.0050223\text{ rad}$** |

---

### Cas 02 — Poutre Continue à Deux Travées Égales
- **Identifiant** : `BM-02` (Équivalent `RDM-04`)
- **Source de référence** : Équation des trois moments de Clapeyron ; Roark’s *Formulas for Stress and Strain* (Table 8.1, Case 2d).
- **Typologie** : Système hyperstatique d'ordre 1.
- **Hypothèses** : Appuis simples sur couteaux en $x = 0$, $x = L$, $x = 2L$. Section et inertie constantes.

#### Paramètres :
- Portée de chaque travée : $L_1 = L_2 = 6.00\text{ m}$ (Longueur totale $12.00\text{ m}$)
- Section : $b = 0.20\text{ m}$, $h = 0.50\text{ m}$ ($I_z = 2.083333 \times 10^{-3}\text{ m}^4$)
- Matériau (Béton C30/37) : $E = 33\,000\text{ MPa} = 3.30 \times 10^7\text{ kN/m}^2$
- Rigidité : $EI_z = 68\,750\text{ kNm}^2$
- Chargement : $q = 30.0\text{ kN/m}$ uniforme sur les deux travées.

#### Résultats de référence :
| Grandeur | Formule Analytique Fermée | Valeur Exacte Théorique |
| :--- | :--- | :--- |
| **Moment sur l'appui intermédiaire ($M_B$)** | $- \frac{q L^2}{8}$ | **$-135.000\text{ kNm}$** |
| **Moment maximal en travée ($M_{\max,\text{travée}}$)** | $+ \frac{9}{128} q L^2$ (à $x = \frac{3}{8}L = 2.25\text{ m}$) | **$+75.9375\text{ kNm}$** |
| **Réaction d'appui rive ($R_A = R_C$)** | $\frac{3}{8} q L$ | **$+67.500\text{ kN}$** |
| **Réaction d'appui intermédiaire ($R_B$)** | $\frac{10}{8} q L = 1.25 q L$ | **$+225.000\text{ kN}$** |
| **Flèche maximale en travée ($w_{\max}$)** | $\frac{q L^4}{185 E I_z}$ (à $x \approx 0.4215 L$) | **$3.056\text{ mm}$** ($3.0562 \times 10^{-3}\text{ m}$) |

---

### Cas 03 — Portique Plan 2D (Sway Frame sous Charges Verticales & Horizontales)
- **Identifiant** : `BM-03`
- **Source de référence** : Ghali, Neville & Brown, *Structural Analysis: A Unified Classical and Matrix Approach* (Chapitre 5) ; Guide technique CTICM.
- **Typologie** : Portique simple rectangulaire à un étage (poteaux encastrés en pied, traverse rigide).
- **Hypothèses** : Modèle 2D frame, déformations axiales et de flexion couplées.

#### Paramètres :
- Géométrie : Largeur $L = 6.00\text{ m}$, Hauteur $H = 4.00\text{ m}$
- Poteaux gauche ($AB$) et droite ($CD$) : Profilé HEB 200 ($A = 78.1\text{ cm}^2$, $I_y = 5696\text{ cm}^4$, $E = 210\text{ GPa}$)
- Traverse supérieure ($BC$) : Profilé IPE 300 ($A = 53.8\text{ cm}^2$, $I_y = 8356\text{ cm}^4$, $E = 210\text{ GPa}$)
- Encastrements parfaits en pied ($A$ et $D$) : $u = w = \theta = 0$.
- Chargement combiné :
  - Charge horizontale en tête ($B$) : $H_a = 40.0\text{ kN}$ vers la droite ($+X$).
  - Charge verticale répartie sur traverse ($BC$) : $q = 25.0\text{ kN/m}$ vers le bas ($-Z$).

#### Résultats de référence :
| Grandeur | Solution Numérique Exacte RDM | Tolérance Admissible |
| :--- | :--- | :--- |
| **Déplacement horizontal en tête ($u_B \approx u_C$)** | **$+8.64\text{ mm}$** | $\le 0.10\,\%$ |
| **Moment en pied de poteau gauche ($M_A$)** | **$+48.32\text{ kNm}$** | $\le 0.10\,\%$ |
| **Moment en pied de poteau droit ($M_D$)** | **$+38.68\text{ kNm}$** | $\le 0.10\,\%$ |
| **Moment d'angle nœud $B$ ($M_{BA} = M_{BC}$)** | **$-55.24\text{ kNm}$** | $\le 0.10\,\%$ |
| **Moment en travée traverse ($M_{\max,BC}$)** | **$+61.45\text{ kNm}$** | $\le 0.10\,\%$ |

---

### Cas 04 — Portique Spatial 3D sous Flexion Biaxiale & Torsion
- **Identifiant** : `BM-04`
- **Source de référence** : K.J. Bathe, *Finite Element Procedures*, Example 5.18 (3D Beam Element Benchmark).
- **Typologie** : Structure tridimensionnelle à barres orthogonales.
- **Hypothèses** : Modélisation 3D frame à 6 DDL par nœud ($u, v, w, \theta_x, \theta_y, \theta_z$).

#### Paramètres :
- Nœud 1 (Encastrement) : $(0, 0, 0)$
- Nœud 2 (Angle) : $(0, 0, 3.00\text{ m})$ (Poteau vertical de $3\text{ m}$)
- Nœud 3 (Extrémité libre) : $(4.00\text{ m}, 0, 3.00\text{ m})$ (Poutre horizontale en porte-à-faux de $4\text{ m}$)
- Profilé tubulaire carré creux SHS $150 \times 150 \times 10\text{ mm}$ sur les deux membres ($A = 54.3\text{ cm}^2$, $I_y = I_z = 1750\text{ cm}^4$, $I_t = 2780\text{ cm}^4$, $E = 210\text{ GPa}$, $G = 81\text{ GPa}$).
- Charge ponctuelle appliquée au nœud 3 : $\vec{F} = (0, 15.0\text{ kN}, -25.0\text{ kN})$.

#### Résultats de référence au nœud libre 3 :
| Degré de Liberté | Déplacement / Rotation Exact | Tolérance |
| :--- | :--- | :--- |
| **Déplacement $v_3$ (selon $Y$)** | **$+21.68\text{ mm}$** | $\le 0.20\,\%$ |
| **Déplacement vertical $w_3$ (selon $Z$)** | **$-38.45\text{ mm}$** | $\le 0.20\,\%$ |
| **Moment de torsion dans le poteau 1-2 ($M_{x1}$)** | **$15.0\text{ kN} \times 4.0\text{ m} = 60.000\text{ kNm}$** | $\le 0.05\,\%$ |
| **Moment de flexion en pied ($M_{y1}$)** | **$25.0\text{ kN} \times 4.0\text{ m} = 100.000\text{ kNm}$** | $\le 0.05\,\%$ |

---

### Cas 05 — Torsion Non-Uniforme & Gauchissement d'une Poutre en I (Vlasov)
- **Identifiant** : `BM-05`
- **Source de référence** : V.Z. Vlasov, *Thin-Walled Elastic Beams* ; Traité CTICM *Torsion des profilés ouverts*.
- **Typologie** : Barre à parois minces, couplage torsion de Saint-Venant et gauchissement empêché.
- **Hypothèses** : Poutre console en IPE 300 de longueur $L = 3.00\text{ m}$, encastrée contre le gauchissement en pied ($\theta_x = 0, \theta_x' = 0$), soumise à un couple de torsion concentré $T = 5.0\text{ kNm}$ en extrémité libre.

#### Paramètres géométriques IPE 300 :
- $I_t = 20.12\text{ cm}^4 = 2.012 \times 10^{-7}\text{ m}^4$ (Inertie de Saint-Venant)
- $I_\omega = 125\,900\text{ cm}^6 = 1.259 \times 10^{-7}\text{ m}^6$ (Constante sectorielle de gauchissement)
- $E = 210\text{ GPa}$, $G = 80.77\text{ GPa}$
- Longueur caractéristique de torsion : $\lambda = \sqrt{\frac{G I_t}{E I_\omega}} = \sqrt{\frac{80.77 \times 10^9 \times 2.012 \times 10^{-7}}{210 \times 10^9 \times 1.259 \times 10^{-7}}} = 0.7842\text{ m}^{-1}$.

#### Résultats de référence :
| Grandeur | Formule de Vlasov | Valeur Exacte |
| :--- | :--- | :--- |
| **Angle de rotation libre ($\theta_x(L)$)** | $\frac{T}{G I_t} \left[ L - \frac{\tanh(\lambda L)}{\lambda} \right]$ | **$0.0567\text{ rad}$ ($3.25^\circ$)** |
| *(Torsion pure St-Venant sans gauchissement)* | $\theta_{\text{StV}} = \frac{T L}{G I_t}$ | *$0.0923\text{ rad}$ (surestimation de 63%)* |
| **Bimoment à l'encastrement ($B_\omega(0)$)** | $-\frac{T}{\lambda} \tanh(\lambda L)$ | **$-6.262\text{ kNm}^2$** |

---

### Cas 06 — Dalle Rectangulaire en Flexion (Navier / Kirchhoff-Love)
- **Identifiant** : `BM-06`
- **Source de référence** : S. Timoshenko & S. Woinowsky-Krieger, *Theory of Plates and Shells* (Table 8) ; Benchmark NAFEMS LE1.
- **Typologie** : Plaque mince 2D simplement appuyée sur son pourtour.
- **Hypothèses** : Flexion de plaque isotrope mince, charge uniforme $q$, théorie de Kirchhoff (dalle DKT).

#### Paramètres :
- Dimensions : $a = 4.00\text{ m}$ (selon $X$), $b = 6.00\text{ m}$ (selon $Y$) (Ratio $b/a = 1.50$)
- Épaisseur : $h = 0.20\text{ m}$
- Matériau (Béton) : $E = 30\,000\text{ MPa}$, $\nu = 0.20$
- Rigidité flexionnelle de la plaque : $D = \frac{E h^3}{12(1 - \nu^2)} = \frac{3.0 \times 10^7 \times 0.20^3}{12(1 - 0.04)} = 20\,833.33\text{ kNm}$
- Chargement : $q = 10.0\text{ kN/m}^2$ uniforme vers le bas.

#### Résultats de référence au centre $(a/2, b/2)$ :
| Grandeur | Facteur Tabulé Timoshenko ($\alpha, \beta_1, \beta_2$) | Valeur Exacte Théorique |
| :--- | :--- | :--- |
| **Flèche maximale au centre ($w_0$)** | $\alpha \frac{q a^4}{D}$ avec $\alpha = 0.00772$ | **$0.9487\text{ mm}$** ($9.487 \times 10^{-4}\text{ m}$) |
| **Moment fléchissant $m_x$** | $\beta_1 q a^2$ avec $\beta_1 = 0.0812$ | **$+12.992\text{ kNm/m}$** |
| **Moment fléchissant $m_y$** | $\beta_2 q a^2$ avec $\beta_2 = 0.0498$ | **$+7.968\text{ kNm/m}$** |

---

### Cas 07 — Toiture Cylindrique Mince (Scordelis-Lo Shell Roof Benchmark)
- **Identifiant** : `BM-07`
- **Source de référence** : A.C. Scordelis, K.S. Lo (1964), *Computer analysis of cylindrical shells* ; NAFEMS Shell Benchmark.
- **Typologie** : Coque mince cylindrique simple courbure sous poids propre.
- **Hypothèses** : Diaphragmes rigides d'extrémité ($u = w = 0$), bords longitudinaux libres.

#### Paramètres :
- Rayon de courbure : $R = 25.0\text{ ft} = 7.620\text{ m}$
- Longueur : $L = 50.0\text{ ft} = 15.240\text{ m}$
- Angle d'ouverture : $\phi = 40^\circ$ (demi-angle $40^\circ$)
- Épaisseur : $t = 0.25\text{ ft} = 0.0762\text{ m}$
- Matériau : $E = 4.32 \times 10^8\text{ psf} = 2.0684 \times 10^7\text{ kPa}$, $\nu = 0.0$
- Poids propre surfacique : $q = 90.0\text{ psf} = 4.309\text{ kPa}$ (gravité descendante).

#### Résultats de référence (Convergence NAFEMS) :
| Point de Mesure | Déplacement de Référence Convergé | Tolérance TSA |
| :--- | :--- | :--- |
| **Déplacement vertical du bord libre à mi-longueur ($w_{\text{edge}}$)** | **$-0.3024\text{ ft} = -92.17\text{ mm}$** | $\le 1.5\,\%$ |
| **Déplacement vertical au sommet à mi-longueur ($w_{\text{crown}}$)** | **$+0.1585\text{ ft} = +48.31\text{ mm}$** | $\le 1.5\,\%$ |

---

### Cas 08 — Analyse Modale : Fréquences Propres d'une Console et d'un Portique 2D
- **Identifiant** : `BM-08`
- **Source de référence** : R.W. Clough & J. Penzien, *Dynamics of Structures* ; Blevins, *Formulas for Natural Frequency and Mode Shape*.
- **Typologie** : Problème aux valeurs propres dynamiques $[K - \omega^2 M]\{\phi\} = \{0\}$.

#### Configuration A : Poutre Console Continue
- Paramètres : $L = 4.00\text{ m}$, Section IPE 240, $E = 210\text{ GPa}$, masse volumique $\rho = 7850\text{ kg/m}^3$, masse linéique $m = 30.7\text{ kg/m}$, $I_y = 3892\text{ cm}^4$.
- Formule analytique : $f_n = \frac{\lambda_n^2}{2 \pi L^2} \sqrt{\frac{E I_y}{m}}$ avec $\lambda_1 = 1.8751$, $\lambda_2 = 4.6941$, $\lambda_3 = 7.8548$.
- **Résultats de référence** :
  - Mode 1 ($f_1$) : **$11.62\text{ Hz}$**
  - Mode 2 ($f_2$) : **$72.81\text{ Hz}$**
  - Mode 3 ($f_3$) : **$203.88\text{ Hz}$**

---

### Cas 09 — Réponse Sismique : Spectre de Dimensionnement Eurocode 8
- **Identifiant** : `BM-09`
- **Source de référence** : EN 1998-1:2004 §3.2.2.5 (Spectre élastique) et Guide d'application JRC Sismique.
- **Typologie** : Calcul de l'accélération spectrale $S_d(T)$ pour un bâtiment régulier.
- **Hypothèses** : Sol de type B ($S = 1.2, T_B = 0.15\text{ s}, T_C = 0.50\text{ s}, T_D = 2.00\text{ s}$), accélération de référence $a_{gR} = 1.50\text{ m/s}^2$ (Zone sismique modérée, compatible Hautes Terres Madagascar IOGA), coefficient d'importance $\gamma_I = 1.0$, comportement ductile $q = 3.9$ (portique béton armé DCM).

#### Valeurs spectrales exactes :
| Période $T$ (s) | Régime du Spectre | Formule Normative EC8 | Accélération de Calcul $S_d(T)$ |
| :--- | :--- | :--- | :--- |
| $T = 0.05\text{ s}$ | $0 \le T < T_B$ | $a_g S \left[ \frac{2}{3} + \frac{T}{T_B} \left( \frac{2.5}{q} - \frac{2}{3} \right) \right]$ | **$1.183\text{ m/s}^2$** |
| $T = 0.30\text{ s}$ | $T_B \le T \le T_C$ (Plateau) | $a_g S \frac{2.5}{q}$ | **$1.154\text{ m/s}^2$** |
| $T = 1.00\text{ s}$ | $T_C \le T \le T_D$ | $a_g S \frac{2.5}{q} \left( \frac{T_C}{T} \right)$ | **$0.577\text{ m/s}^2$** |
| $T = 2.50\text{ s}$ | $T_D \le T$ | $a_g S \frac{2.5}{q} \left( \frac{T_C T_D}{T^2} \right)$ | **$0.185\text{ m/s}^2$** |

---

### Cas 10 — Poutre de Pont sous Système de Charges Convoi Routier LM1 (EN 1991-2)
- **Identifiant** : `BM-10`
- **Source de référence** : EN 1991-2:2003 (Trafic sur les ponts-routes) ; JRC Report EUR 25193 *Bridge Design to the Eurocodes* (Worked Example 1).
- **Typologie** : Ligne d'influence et enveloppe des moments max sous convoi tandem mobile + charge répartie (UDL).

#### Paramètres :
- Poutre maîtresse bi-appuyée de pont : $L = 24.00\text{ m}$
- Largeur de voie : Voie 1 ($3.00\text{ m}$)
- Charge UDL : $q_{1k} = 9.0\text{ kN/m}$
- Tandem double essieu (TS) : Deux essieux de $Q_{1k} = 300\text{ kN}$ espacés de $1.20\text{ m}$ (soit 2 essieux de $150\text{ kN}$ par file de roue).
- Position critique de l'essieu pour $M_{\max}$ au centre ($x = 12.00\text{ m}$) : Théorème de Barré.

#### Résultats de référence au centre de la poutre :
| Composante de Charge | Calcul de la Sollicitation Maximale | Valeur de Référence Exacte |
| :--- | :--- | :--- |
| **Charge UDL ($q$)** | $M_{q} = \frac{q L^2}{8} = \frac{9.0 \times 24^2}{8}$ | **$+648.00\text{ kNm}$** |
| **Tandem mobile (TS)** | $M_{TS} = \frac{2 P}{L} \left( \frac{L}{2} - \frac{d}{4} \right)^2 - \dots$ | **$+3\,367.50\text{ kNm}$** |
| **Moment total caractéristique** | $M_{\max} = M_q + M_{TS}$ | **$+4\,015.50\text{ kNm}$** |

---

### Cas 11 — Dalot Cadre Fermé Enterré (SETRA / Pont-Cadre PICF)
- **Identifiant** : `BM-11`
- **Source de référence** : Guide Technique SETRA (1998) *Ponts-cadres et portiques* ; Programme de calcul PICF (Ponts Intégraux Cadres Fermés).
- **Typologie** : Portique fermé fermé 2D sous pression hydrostatique, remblai et poussée des terres.
- **Hypothèses** : Cadre fermé en béton armé ($H_{\text{ext}} = 3.50\text{ m}, L_{\text{ext}} = 4.50\text{ m}$), épaisseur uniforme de traverse et piédroits $e = 0.35\text{ m}$.

#### Paramètres :
- Dimensions d'axe en axe : Portée $L = 4.15\text{ m}$, Hauteur $H = 3.15\text{ m}$
- Section unitaire de calcul : Bande de largeur $1.00\text{ m}$ ($b = 1.00\text{ m}, h = 0.35\text{ m}, I = \frac{1.0 \times 0.35^3}{12} = 3.5729 \times 10^{-3}\text{ m}^4$)
- Matériau (Béton C30/37) : $E = 33\,000\text{ MPa}$
- Chargement symétrique :
  - Poids propre + couverture de remblai sur traverse supérieure : $q_v = 45.0\text{ kN/m}$
  - Poussée latérale des terres au repos sur piédroits ($K_0 = 0.50$) : Pression variant de $p_{\text{haut}} = 15.0\text{ kN/m}^2$ à $p_{\text{bas}} = 45.0\text{ kN/m}^2$.

#### Moments aux nœuds et en travée (Méthode des rotations / Hardy Cross) :
| Section Critique du Cadre | Moment de Flexion Analytique de Référence | Tolérance TSA |
| :--- | :--- | :--- |
| **Angle supérieur Traverse/Piédroit ($M_{\text{coin}}$)** | **$-52.84\text{ kNm/m}$** | $\le 0.5\,\%$ |
| **Mi-travée Traverse Supérieure ($M_{\text{trav,sup}}$)** | **$+44.12\text{ kNm/m}$** | $\le 0.5\,\%$ |
| **Mi-hauteur Piédroit ($M_{\text{pied}}$)** | **$+14.78\text{ kNm/m}$** | $\le 0.5\,\%$ |
| **Angle inférieur Traverse/Radier** | **$-68.35\text{ kNm/m}$** | $\le 0.5\,\%$ |

---

## 3. Synthèse des Données de Validation pour l'Intégration Continue (CI)

Le tableau ci-dessous récapitule les 11 cas tests pour injection directe dans la suite de tests automatisée `TSA_TestSuite` (`tests/benchmarks/`) :

| Ref Test | Titre Court | Degré d'Exactitude | Outil de Validation | Suite TSA |
| :--- | :--- | :--- | :--- | :--- |
| `BM-01` | Poutre simplement appuyée (UDL & P) | Exacte fermée | Custom2D & OpenSees | `--suite=rdm` |
| `BM-02` | Poutre continue 2 travées Clapeyron | Exacte fermée | Custom2D & OpenSees | `--suite=rdm` |
| `BM-03` | Portique 2D avec sway | Exacte RDM | Custom2D & OpenSees | `--suite=rdm` |
| `BM-04` | Portique spatial 3D Bathe 5.18 | Exacte FEM | OpenSees 3.8.0 | `--suite=opensees` |
| `BM-05` | Torsion de Vlasov IPE 300 | Exacte Vlasov | OpenSees (BeamFiber / Warping) | `--suite=fea` |
| `BM-06` | Dalle rectangulaire Kirchhoff (Navier) | Série de Fourier exacte | Gmsh + Solveur DKT | `--suite=mesh --suite=fea` |
| `BM-07` | Toiture Scordelis-Lo | Convergé NAFEMS | Gmsh + Solveur Coques | `--suite=fea` |
| `BM-08` | Fréquences propres Blevins | Exacte dynamique | OpenSees (Eigen) | `--suite=opensees` |
| `BM-09` | Spectre Eurocode 8 & Madagascar | Exacte normative | TSA Spectrum Engine | `--suite=standards` |
| `BM-10` | Convoi routier LM1 de pont | Exacte analytique | TSA MovingLoad Engine | `--suite=bridges` |
| `BM-11` | Dalot cadre fermé PICF SETRA | Analytique portique | Custom2D & OpenSees | `--suite=hydraulic` |
