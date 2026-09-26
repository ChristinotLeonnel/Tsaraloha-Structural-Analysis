# RÉFÉRENTIEL ET NORMES TECHNIQUES DU SYSTÈME DE CÂBLES

Le module de câbles de TSA intègre un registre centralisé (`CableStandardsRegistry`) référençant les caractéristiques mécaniques et géométriques certifiées par les normes internationales de référence en génie civil.

---

## 1. NF EN 10138-3 — Aciers de Précontrainte : Torons 7 Fils

Applicable aux torons à très haute limite élastique utilisés en précontrainte intérieure/extérieure et dans les haubans multitorons (PSS - Parallel Strand System).

| Désignation | Diamètre Nominal $d$ (mm) | Section Nominale $A_p$ (mm²) | Masse Linéique $M$ (g/m) | Résistance Caractéristique $f_{pk}$ (MPa) | Limite d'Élasticité Conventionnelle $f_{p0,1k}$ (MPa) | Module d'Élasticité $E$ (GPa) | Relaxation à 1000h $\rho_{1000}$ (%) |
|---|---|---|---|---|---|---|---|
| **Y1860S7-15.7** | 15.7 | 150.0 | 1172 | 1860 | 1600 | 195 | $\le 2.5$ |
| **Y1860S7-15.3** | 15.3 | 140.0 | 1093 | 1860 | 1600 | 195 | $\le 2.5$ |
| **Y1860S7-15.2** | 15.2 | 139.0 | 1086 | 1860 | 1600 | 195 | $\le 2.5$ |
| **Y1860S7-12.7** | 12.7 | 98.7 | 771 | 1860 | 1600 | 195 | $\le 2.5$ |
| **Y1860S7-12.5** | 12.5 | 93.0 | 726 | 1860 | 1600 | 195 | $\le 2.5$ |
| **Y1860S7-9.3**  | 9.3 | 52.0 | 406 | 1860 | 1600 | 195 | $\le 2.5$ |
| **Y1770S7-15.7** | 15.7 | 150.0 | 1172 | 1770 | 1520 | 195 | $\le 2.5$ |
| **Y1770S7-15.2** | 15.2 | 139.0 | 1086 | 1770 | 1520 | 195 | $\le 2.5$ |

---

## 2. NF EN 10138-4 — Aciers de Précontrainte : Barres Haute Résistance

Applicable aux barres massives lisses ou à filetage continu laminé à chaud, principalement utilisées comme tirants d'ancrage, barres de suspension et précontrainte d'assemblage.

| Désignation | Diamètre Nominal $d$ (mm) | Section Nominale $S_n$ (mm²) | Masse Linéique (kg/m) | Résistance Caractéristique $f_{pk}$ (MPa) | Limite d'Élasticité $f_{p0,1k}$ (MPa) | Module d'Élasticité $E$ (GPa) | Allongement sous charge max $A_{gt}$ (%) |
|---|---|---|---|---|---|---|---|
| **Y1030-26** | 26.5 | 551.5 | 4.48 | 1030 | 835 | 205 | $\ge 3.5$ |
| **Y1030-32** | 32.0 | 804.2 | 6.53 | 1030 | 835 | 205 | $\ge 3.5$ |
| **Y1030-36** | 36.0 | 1017.9 | 8.27 | 1030 | 835 | 205 | $\ge 3.5$ |
| **Y1030-40** | 40.0 | 1256.6 | 10.21 | 1030 | 835 | 205 | $\ge 3.5$ |
| **Y1030-47** | 47.0 | 1734.9 | 14.10 | 1030 | 835 | 205 | $\ge 3.5$ |

---

## 3. NF EN 1993-1-11 — Eurocode 3 : Calcul des Structures en Câbles

Définit les règles de calcul et les modules d'élasticité stabilisés après pré-étirage (*pre-stretching*).

### A. Câbles Clos (Full-Locked Coil Ropes - FLC)
Câbles constitués de fils ronds intérieurs entourés d'une ou plusieurs couches de fils en Z ou trapézoïdaux imbriqués, conférant une étanchéité exceptionnelle et un facteur de remplissage élevé (~0.78).
- **Module d'élasticité stabilisé** : $E = 160 \pm 10\text{ GPa}$
- **Résistance nominale des fils élémentaires** : $R_r = 1570\text{ MPa}$ ou $1770\text{ MPa}$
- **Diamètres standards pré-configurés** : $\varnothing 50$, $\varnothing 70$, $\varnothing 90$, $\varnothing 120$, $\varnothing 140\text{ mm}$

### B. Câbles Toronnés Hélicoïdaux (Spiral Strand Ropes - SSR)
Constitués uniquement de couches hélicoïdales de fils ronds.
- **Module d'élasticité stabilisé** : $E = 150 \pm 10\text{ GPa}$
- **Facteur de remplissage** : ~0.60 à 0.65
- **Diamètres standards pré-configurés** : $\varnothing 20$, $\varnothing 32$, $\varnothing 44$, $\varnothing 56$, $\varnothing 64\text{ mm}$

### C. Faisceaux de Torons Parallèles (Parallel Strand System - PSS)
Système universel de haubans modernes (ex: Freyssinet HD, DYWIDAG, VSL SSI).
- Constitué de $N$ torons galvanisés sous gaine PEHD individuelle :
  $$A_{tot} = N \cdot A_{toron}$$
- **Module d'élasticité** : $E = 195\text{ GPa}$
- **Unités standards** : 7, 12, 19, 27, 37, 55, 61, 75, 91 torons T15S (15.7 mm).

---

## 4. ASTM A416 / AASHTO — Seven-Wire Steel Strand for Prestressed Concrete

| Désignation | Diamètre (in / mm) | Section $A$ (in² / mm²) | Résistance Ultime $f_{pu}$ (ksi / MPa) | Limite Élastique $f_{py}$ (ksi / MPa) | Module $E$ (ksi / GPa) |
|---|---|---|---|---|---|
| **Gr 270 - 0.5 in** | 0.500 in (12.7 mm) | 0.153 in² (98.7 mm²) | 270 ksi (1860 MPa) | 243 ksi (1674 MPa) | 28500 ksi (196.5 GPa) |
| **Gr 270 - 0.6 in** | 0.600 in (15.24 mm) | 0.217 in² (140.0 mm²) | 270 ksi (1860 MPa) | 243 ksi (1674 MPa) | 28500 ksi (196.5 GPa) |
