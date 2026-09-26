# PRÉCONTRAINTE, TENSION INITIALE & PERTES

La modélisation de la mise en tension des câbles et tirants dans **TSA** est gérée par la structure [CablePrestress](file:///E:/Book/Dev/TSA/src/Model/Cable/CablePrestress.h#L11-L60), conforme aux préconisations des normes **EN 1992-1-1** (Eurocode 2, Chapitre 5.10) et **EN 1993-1-11** (Eurocode 3).

---

## 1. Paramètres de Mise en Tension Initiale

À l'état initial, le câble est caractérisé par un effort de traction imposé ou une déformation initiale :

- **Force de mise en tension initiale $P_0$ (ou $T_0$)** : Exprimée en Newtons ($\text{N}$).
- **Déformation initiale relative $\varepsilon_{init}$** :
  $$\varepsilon_{init} = \frac{P_0}{E_{cable} \cdot A_{metal}}$$
- **Contrainte initiale $\sigma_0$** :
  $$\sigma_0 = \frac{P_0}{A_{metal}}$$

Vérification à la mise en tension selon l'Eurocode 2 (Article 5.10.2.1) :
$$\sigma_{max} \le \min(0.80 \cdot f_{pk}, \; 0.90 \cdot f_{p0.1k})$$

---

## 2. Pertes Instantanées de Tension

### 2.1 Pertes par Frottement le long de la Trajectoire
Pour les câbles présentant une courbure (câbles porteurs déviés, haubans avec déviateurs ou câbles de précontrainte intérieure/extérieure) :
$$P(x) = P_0 \cdot e^{-(\mu \cdot \theta(x) + k \cdot x)}$$
La perte de tension à l'abscisse $x$ s'écrit :
$$\Delta P_\mu(x) = P_0 \cdot \left(1 - e^{-(\mu \cdot \theta(x) + k \cdot x)}\right)$$
où :
- $\mu$ : Coefficient de frottement angulaire entre le câble et son conduit ou déviateur ($\text{rad}^{-1}$).
  - Câble sous gaine PEHD lubrifiée : $\mu \approx 0.05 - 0.12$.
  - Câble dans conduit métallique injecté : $\mu \approx 0.15 - 0.24$.
- $\theta(x)$ : Somme des déviations angulaires absolues sur la longueur $x$ ($\text{rad}$).
- $k$ : Coefficient d'ondulation parasite (*wobble*) par unité de longueur ($\text{m}^{-1}$ ou $\text{rad/m}$, couramment $0.005 - 0.010\text{ m}^{-1}$).
- $x$ : Distance le long du câble depuis l'ancrage actif.

### 2.2 Pertes par Rentrée d'Ancrage (*Anchorage Seating Loss*)
Lors du transfert de l'effort du vérin vers les mors d'ancrage, un glissement mécanique $\Delta L_{slip}$ se produit (généralement de $4\text{ mm}$ à $8\text{ mm}$ pour les torons, $1\text{ mm}$ à $2\text{ mm}$ pour les barres filetées) :

- L'effort décroît sur une longueur d'influence $l_{set}$ :
  $$l_{set} = \sqrt{\frac{\Delta L_{slip} \cdot E \cdot A_{metal}}{w_{loss}}}$$
  où $w_{loss} = \left|\frac{dP}{dx}\right|$ est le taux de perte linéique par frottement au voisinage de l'ancrage.
- Perte maximale à l'ancrage :
  $$\Delta P_{slip}(0) = 2 \cdot w_{loss} \cdot l_{set} = 2 \cdot \sqrt{\Delta L_{slip} \cdot E \cdot A_{metal} \cdot w_{loss}}$$

---

## 3. Pertes Différées dans le Temps

1. **Relaxation Isotherme de l'Acier à Haute Résistance** :
   Calculée selon la classe de relaxation du toron (Classe 1 : relaxation normale, Classe 2 : très basse relaxation TBR selon EN 10138-3) :
   $$\frac{\Delta \sigma_{pr}}{\sigma_{p0}} = 0.66 \cdot \rho_{1000} \cdot e^{9.1 \cdot \mu_{rel}} \cdot \left(\frac{t}{1000}\right)^{0.75 \cdot (1 - \mu_{rel})} \cdot 10^{-5}$$
   avec $\mu_{rel} = \frac{\sigma_{p0}}{f_{pk}}$ et $\rho_{1000} \approx 2.5\%$ pour la classe 2.
2. **Retrait et Fluage du Support (Béton / Pylônes)** :
   Modélisés par raccourcissement élasto-visqueux transmis aux ancrages d'extrémités.

---

## 4. Intégration dans le Moteur de Calcul TSA

- Le module [CablePrestress](file:///E:/Book/Dev/TSA/src/Model/Cable/CablePrestress.h) fournit les fonctions d'évaluation analytique directe des tensions résiduelles après pertes.
- Les forces équivalentes aux extrémités sont injectées dans le vecteur des forces nodales nodales globales lors de la phase d'assemblage du solveur élément fini.
