# Référentiel des Ponts & Ouvrages d'Art (Bridges & Infrastructure)

## 1. Typologies des Ouvrages d'Art

La conception et la modélisation des ponts dans TSA s'appuie sur les typologies classiques du génie civil et les guides méthodologiques du **SETRA / Cerema** :

| Famille d'Ouvrage | Sigle | Portées Types | Modélisation TSA | Documents de Référence |
| :--- | :--- | :--- | :--- | :--- |
| **Ponts-Dalles en Béton Armé / Précontraint** | **PSI-DA / PS-DP** | $10\text{ à }30\text{ m}$ | Éléments surfaciques 2D (Mindlin/DKT) ou grillage de poutres équivalentes | Guide SETRA *Ponts-dalles* (1989) |
| **Poutres Préfabriquées Précontraintes** | **VIPP / PRAD** | $25\text{ à }50\text{ m}$ | Éléments poutres 3D avec câbles de précontrainte adhérents/non-adhérents | Guide SETRA *VIPP* (1996) |
| **Ponts Mixtes Bipoutres Acier-Béton** | **PRM** | $35\text{ à }100\text{ m}$ | Poutres métalliques en I connectées par goujons à la dalle supérieure | Guide SETRA *Ponts mixtes bipoutres* (1998) |
| **Ponts en Caisson (Béton Précontraint)** | **PSI-OM** | $40\text{ à }150\text{ m}$ | Éléments volumiques B-Rep OCCT + éléments de poutre avec torsion/gauchissement | Guide SETRA *Ponts en caisson* |
| **Ponts à Haubans** | **HBN** | $100\text{ à }1000\text{ m}$ | Pylônes 3D, tablier et éléments [`StayCable`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Cable/StayCable.h) sous tension initiale | Guide SETRA *Haubans* / FIB Bulletin 89 |
| **Ponts Suspendus** | **SUSP** | $> 300\text{ m}$ | Câbles porteurs paraboliques, suspentes verticales et tablier treillis | Formulations caténaires d'Irvine |

---

## 2. Modèles de Charges Routières selon l'Eurocode 1 (EN 1991-2)

Le chargement d'un pont routier combine des véhicules lourds concentrés et des surcharges réparties sur des voies de circulation conventionnelles de largeur $w_l = 3.00\text{ m}$.

### 2.1 Modèle de Charge 1 (LM1) — Trafic Général & Régulier
Application conjointe d'un système de boggie tandem (TS) et d'un système de charge uniformément répartie (UDL) :

| Voie de Circulation | Tandem TS (2 essieux à 1.20 m) | Surcharge Uniforme UDL ($q_{ik}$) |
| :--- | :---: | :---: |
| **Voie n° 1** | $2 \times 150\text{ kN} = 300\text{ kN}$ | $9.0\text{ kN/m}^2$ |
| **Voie n° 2** | $2 \times 100\text{ kN} = 200\text{ kN}$ | $2.5\text{ kN/m}^2$ |
| **Voie n° 3** | $2 \times 50\text{ kN} = 100\text{ kN}$ | $2.5\text{ kN/m}^2$ |
| **Zone résiduelle ($q_{rk}$)** | $0$ | $2.5\text{ kN/m}^2$ |

*Empattement des essieux du tandem :* Entraxe longitudinal de $1.20\text{ m}$, voie transversale de $2.00\text{ m}$, surface de contact carré de $0.40 \times 0.40\text{ m}$.

### 2.2 Modèle de Charge 2 (LM2) — Essieu Lourd Unique Isolé
* Essieu unique de charge totale $Q_{ak} = 400\text{ kN}$ appliqué sur deux roues de $200\text{ kN}$ distantes de $2.00\text{ m}$ (surface de contact $0.35 \times 0.60\text{ m}$).
* Conçu pour le dimensionnement des éléments locaux du tablier (dalle de couverture, entretoises, porte-à-faux).

### 2.3 Efforts Horizontaux (Freinage et Force Centrifuge)
* **Effort longitudinal de freinage $Q_{lk}$ :**
  $$Q_{lk} = 0.6 \times (2 Q_{1k}) + 0.10 \times q_{1k} \times w_1 \times L \quad (180\text{ kN} \le Q_{lk} \le 900\text{ kN})$$
* **Force centrifuge $Q_{tk}$ :** Dépend du rayon de courbure en plan $R$ et de la vitesse de référence.

---

## 3. Éléments Câbles, Haubans et Précontrainte dans TSA

Pour les ponts à haubans et passerelles piétonnes, le comportement des câbles est non-linéaire en raison de la **flèche caténaire** (*sagging effect*).

### Module d'Équivalence d'Ernst
La raideur apparente d'un câble incliné sous tension de traction $T$ s'exprime par le module sécant d'Ernst :

$$E_{eq} = \frac{E}{1 + \frac{(\gamma L_h)^2 E}{12 \sigma^3}}$$

où :
* $E$ est le module élastique vrai du toron métallique ($\approx 160\text{ à }195\text{ GPa}$).
* $\gamma = \rho g$ est le poids volumique du câble ($\approx 77\text{ kN/m}^3$).
* $L_h$ est la projection horizontale de la portée du câble.
* $\sigma = T / A$ est la contrainte de traction en service.

Dans TSA, ce comportement est encapsulé dans [`src/Model/Cable/Cable.h`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Cable/Cable.h) et piloté par [`src/Model/Cable/StayCable.h`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/Cable/StayCable.h).

---

## 4. Guides Techniques Officiels

1. **SETRA / Cerema** — *Ponts-routes à poutres préfabriquées précontraintes par adhérence (PRAD)*, Guide de conception.
2. **SETRA / Cerema** — *Ponts mixtes acier-béton : bipoutres*, Guide de conception.
3. **JRC (European Commission)** — *Bridge Design to the Eurocodes: Worked Examples*, JRC Scientific and Technical Reports, EUR 25193 EN.
4. **FIB (Fédération Internationale du Béton)** — *Bulletin 89: Cable-stayed bridges*, 2019.
