# Référentiel des Normes Eurocodes (CEN / EN 1990 à EN 1999)

## 1. Structure du Corpus Normatif Européen

Les Eurocodes constituent le système de normes de conception et de calcul de génie civil et de bâtiment harmonisé en Europe, adopté également comme référence internationale en Afrique et à Madagascar.

| Norme | Désignation | Parties Principales | Intégration dans TSA |
| :--- | :--- | :--- | :--- |
| **EN 1990** | **Eurocode 0 : Bases de calcul des structures** | EN 1990:2002 + A1:2005 (et nouvelle génération prEN 1990:2023) | Moteur de combinaisons de charges ELU / ELS ([`LoadManager`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Load/LoadManager.h)) |
| **EN 1991** | **Eurocode 1 : Actions sur les structures** | EN 1991-1-1 (Poids/Exploitation), EN 1991-1-3 (Neige), EN 1991-1-4 (Vent), EN 1991-1-5 (Thermique) | Préréglages de charges et générateur climatique |
| **EN 1992** | **Eurocode 2 : Structures en béton** | EN 1992-1-1:2004 + A1:2014 (Bâtiment/Génie civil), EN 1992-2 (Ponts) | Module de dimensionnement béton armé ([`ConcreteDesignEC2`](file:///d:/Tsaraloha%20Platforme/TSA/src/Standards/Design/ConcreteDesignEC2.h)) |
| **EN 1993** | **Eurocode 3 : Structures en acier** | EN 1993-1-1:2005 + A1:2014 (Règles générales), EN 1993-1-5 (Plaques), EN 1993-1-8 (Assemblages) | Classification profilés, vérification flambement/déversement ([`SteelDesignEC3`](file:///d:/Tsaraloha%20Platforme/TSA/src/Standards/Design/SteelDesignEC3.h)) |
| **EN 1994** | **Eurocode 4 : Structures mixtes acier-béton** | EN 1994-1-1 (Poutres et planchers collaborants), EN 1994-2 (Ponts mixtes) | Poutres mixtes et dalles collaborantes |
| **EN 1997** | **Eurocode 7 : Calcul géotechnique** | EN 1997-1:2004 + A1:2013 (Règles générales fondations), EN 1997-2 (Essais) | Dimensionnement des semelles et radiers ([`Foundation`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Foundation.h)) |
| **EN 1998** | **Eurocode 8 : Résistance aux séismes** | EN 1998-1:2004 + A1:2013 (Bâtiments, spectres), EN 1998-2 (Ponts), EN 1998-5 (Fondations) | Spectre de dimensionnement et analyse modale |

---

## 2. Eurocode 0 (EN 1990) — Combinaisons d'Actions

### 2.1 États Limites Ultimes (ELU) — Situations Durables et Transitoires
Formulation générale selon l'Expression 6.10 :

$$\sum_{j \ge 1} \gamma_{G,j} G_{k,j} + \gamma_P P + \gamma_{Q,1} Q_{k,1} + \sum_{i > 1} \gamma_{Q,i} \psi_{0,i} Q_{k,i}$$

* Coefficients partiels standards (bâtiment courant) :
  * $\gamma_{G,sup} = 1.35$ (actions permanentes défavorables), $\gamma_{G,inf} = 1.00$ (favorables).
  * $\gamma_{Q,1} = 1.50$ (action variable dominante).
  * $\gamma_{Q,i} = 1.50$ avec facteur de combinaison $\psi_{0,i}$ pour les actions d'accompagnement.

### 2.2 Coefficients de Simultanéité $\psi$ Standards (Tableau A1.1)

| Catégorie d'Action Variable ($Q$) | $\psi_0$ (Combinaison) | $\psi_1$ (Fréquente) | $\psi_2$ (Quasi-permanente) |
| :--- | :---: | :---: | :---: |
| **Habitation / Bureaux (Cat. A, B)** | 0.7 | 0.5 | 0.3 |
| **Commerce / Rassemblement (Cat. C, D)** | 0.7 | 0.7 | 0.6 |
| **Stockage (Cat. E)** | 1.0 | 0.9 | 0.8 |
| **Trafic VL < 30 kN (Cat. F)** | 0.7 | 0.7 | 0.6 |
| **Charges de Neige (Altitude < 1000 m)** | 0.5 | 0.2 | 0 |
| **Charges de Vent ($W$)** | 0.6 | 0.2 | 0 |
| **Variations de Température ($T$)** | 0.6 | 0.5 | 0 |

### 2.3 États Limites de Service (ELS)
* **Combinaison caractéristique (flèches irréversibles) :** $\sum G_{k,j} + Q_{k,1} + \sum \psi_{0,i} Q_{k,i}$
* **Combinaison fréquente (fissuration béton précontraint) :** $\sum G_{k,j} + \psi_{1,1} Q_{k,1} + \sum \psi_{2,i} Q_{k,i}$
* **Combinaison quasi-permanente (fluage, fissuration béton armé) :** $\sum G_{k,j} + \sum \psi_{2,i} Q_{k,i}$

---

## 3. Eurocode 2 (EN 1992-1-1) — Béton Armé

### 3.1 Diagrammes de Calcul des Matériaux
- **Béton en compression :**
  $$f_{cd} = \alpha_{cc} \frac{f_{ck}}{\gamma_c} \quad (\alpha_{cc} = 1.0 \text{ ou } 0.85, \ \gamma_c = 1.50 \text{ en situation durable})$$
  Diagramme parabole-rectangle ou rectangulaire simplifié ($\lambda = 0.8, \eta = 1.0$ pour $f_{ck} \le 50\text{ MPa}$).
- **Acier de ferraillage passif :**
  $$f_{yd} = \frac{f_{yk}}{\gamma_s} \quad (\gamma_s = 1.15 \text{ en situation durable})$$

### 3.2 Vérification de l'Effort Tranchant
- **Sans armatures d'effort tranchant (dalles, semelles) :**
  $$V_{Rd,c} = \left[ C_{Rd,c} k (100 \rho_l f_{ck})^{1/3} + k_1 \sigma_{cp} \right] b_w d \ge (v_{min} + k_1 \sigma_{cp}) b_w d$$
  avec $k = 1 + \sqrt{\frac{200}{d}} \le 2.0$.
- **Avec armatures d'effort tranchant (modèle du treillis à inclinaison variable $\theta$) :**
  $$V_{Rd,s} = \frac{A_{sw}}{s} z f_{ywd} \cot\theta \quad (1.0 \le \cot\theta \le 2.5)$$
  $$V_{Rd,max} = \alpha_{cw} b_w z \nu_1 f_{cd} \frac{1}{\cot\theta + \tan\theta}$$

---

## 4. Eurocode 3 (EN 1993-1-1) — Structures en Acier

### 4.1 Classification des Sections Transversales
Basée sur le paramètre $\epsilon = \sqrt{\frac{235}{f_y}}$ ($f_y$ en $MPa$) :
- **Classe 1 (Plastique) :** Formation d'une rotule plastique avec capacité de rotation suffisante pour l'analyse plastique globale.
- **Classe 2 (Compacte) :** Résistance plastique atteinte ($M_{pl,Rd} = W_{pl} f_y / \gamma_{M0}$), rotation limitée par le voilement local.
- **Classe 3 (Élastique) :** Contrainte extrême atteignant $f_y$, voilement local empêchant d'atteindre le moment plastique ($M_{el,Rd} = W_{el} f_y / \gamma_{M0}$).
- **Classe 4 (Mince) :** Voilement local dans le domaine élastique avant atteinte de $f_y$ (calcul avec section efficace $A_{eff}, W_{eff}$).

### 4.2 Instabilités des Barres (Flambement & Déversement)
- **Flambement par flexion (Article 6.3.1) :**
  $$N_{b,Rd} = \frac{\chi A f_y}{\gamma_{M1}} \quad \text{avec} \quad \chi = \frac{1}{\Phi + \sqrt{\Phi^2 - \bar{\lambda}^2}} \le 1.0$$
  $$\Phi = 0.5 \left[ 1 + \alpha (\bar{\lambda} - 0.2) + \bar{\lambda}^2 \right], \quad \bar{\lambda} = \sqrt{\frac{A f_y}{N_{cr}}} = \frac{L_{cr}}{i} \frac{1}{\pi \sqrt{\frac{E}{f_y}}}$$
  où $\alpha$ est le facteur d'imperfection selon la courbe de flambement ($a_0, a, b, c, d$).

- **Déversement sous flexion (Article 6.3.2) :**
  $$M_{b,Rd} = \frac{\chi_{LT} W_y f_y}{\gamma_{M1}}$$
  calculé à partir du moment critique élastique $M_{cr}$ prenant en compte le gauchissement et les conditions de maintien latéral des semelles.

---

## 5. Eurocode 7 (EN 1997-1) — Fondations Superficielles

Capacité portante du sol sous semelle filante ou isolée en conditions drainées :

$$\frac{R}{A'} = c' N_c s_c i_c + q' N_q s_q i_q + \frac{1}{2} \gamma' B' N_\gamma s_\gamma i_\gamma$$

avec les coefficients de portance de Prandtl / Reissner :
$$N_q = e^{\pi \tan\phi'} \tan^2\left(45^\circ + \frac{\phi'}{2}\right), \quad N_c = (N_q - 1)\cot\phi', \quad N_\gamma = 2(N_q - 1)\tan\phi'$$

---

## 6. Publications Officielles et Guides d'Application Téléchargeables

* **European Commission Joint Research Centre (JRC) :**
  - *Bridge Design to the Eurocodes – Worked Examples* (JRC Scientific and Technical Reports, EUR 25193 EN).
  - *Design of Steel Buildings with Worked Examples* (JRC EUR 27348 EN).
  - *Design of Concrete Buildings with Worked Examples* (JRC EUR 25204 EN).
* **CEREMA / SETRA (France) :**
  - *Eurocodes 0 et 1 : Application aux ponts-routes.*
  - *Guide d'application de l'Eurocode 2 pour les ponts en béton.*
