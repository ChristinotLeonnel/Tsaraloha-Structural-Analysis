# Projets de démonstration TSA

Projets au format natif `.tsa`, construits par l'API de TSA (`Model`, `Section`, `Material`, `GridManager`,
`TSAFileWriter`) dans la suite de tests `demo` (`tests/test_demo_models.cpp`). Exemples pédagogiques : ils ne sont
pas destinés à la construction.

## Quatre projets complets (tests 350 à 353)

| Fichier | Ouvrage | Nœuds | Barres | Câbles | Dalles / voiles / semelles | Appuis |
|---|---|---|---|---|---|---|
| `TSA_Projet_Batiment_BA_R+4.tsa` | Bâtiment de bureaux R+4 en béton armé | 224 | 493 | — | 86 / 15 / 28 | 30 |
| `TSA_Projet_Batiment_Mixte_BA_Acier.tsa` | Bâtiment mixte béton armé et acier | 210 | 426 | — | 24 / 9 / 15 | 15 |
| `TSA_Projet_Batiment_Complexe_L.tsa` | Bâtiment complexe en L | 227 | 467 | — | 49 / 15 / 33 | 33 |
| `TSA_Projet_Pont_Haubans_80-180-80m.tsa` | Pont à haubans 80 + 180 + 80 m | 229 | 504 | 64 | — / — / 4 | 12 |

**1. Bâtiment en béton armé R+4.** C30/37, trame 6 × 3 travées (5 m × 6 m), niveaux 0 à 16,3 m. Semelles isolées
1,8 × 1,8 m, longrines 30 × 50, poteaux 40 × 40 puis 35 × 35, poutres porteuses 30 × 55 (selon X) et secondaires
25 × 45, dalles de 20 cm (22 cm en toiture), noyau en voiles de 20 cm, escalier (limons, paliers, chevêtres) dans une
trémie. Charges : G = 6,5 kN/m² (7,0 en toiture), Q = 2,5 kN/m² (1,0 en toiture) reportées sur les poutres porteuses
(largeur de reprise 6 m, 3 m en rive).

**2. Bâtiment mixte.** Poteaux 45 × 45 et noyau en béton armé ; planchers R+1 à R+3 à solives IPE 400 (9 m) aux tiers
des travées de 8 m, poutres de file en caisson soudé 300 × 700, dalle sur bac de 14 cm **sans action mixte** ; palées
en tubes 168 × 8 ; grande salle au dernier niveau sous fermes Warren de 18 m (HEA 200, tubes 114 × 6), pannes IPE 200.
G = 5,0 kN/m², Q = 3,0 kN/m² (entraxe des solives 2,67 m).

**3. Bâtiment complexe en L.** Aile A en béton armé R+4 (portiques, noyau, atrium, porte-à-faux de 4 m aux trois derniers
niveaux) ; aile B en acier R+2 à hauteurs d'étage différentes (4,2 m), palées, toiture en arcs polygonaux de 12 m.
Cas G, Q et vent W (charges nodales en façade sud), combinaisons ELU (1,35 G + 1,5 Q + 0,9 W) et ELS.

**4. Pont à haubans.** Travées 80 + 180 + 80 m, tablier de 20 m (poutres de rive en caisson 1 200 × 2 500, pièces de pont
tous les 10 m, longeron, contreventement en croix), pylônes en H en béton armé (fûts en caisson 4,0 × 3,0, entretoises),
64 haubans PSS 37T15,7 en semi-harpe, piles auxiliaires et culées. Tension initiale T0 = g × 10 m / sin θ (part du poids
permanent d'un plan reprise par le hauban, sans réglage itératif). G superstructure 8,75 kN/m², Q trafic 4,0 kN/m².
Les haubans sont des éléments câble (corotTruss, déformation initiale ε0 = T0 / (E·A)) ; le calcul étant linéaire, leur
tension est contrôlée a posteriori (tous tendus sous ELU : 1 108 à 3 850 kN, N / Fpk ≤ 0,37).

Hypothèses communes : analyse statique linéaire (OpenSees 3.8.0), sans second ordre ni analyse modale ; dalles, voiles
et semelles modélisés pour la géométrie et le rendu mais **non transmis au calcul** (leurs charges sont reportées sur les
poutres) ; ces surfaces sont déclarées « non structurales » pour qu'un calcul avec maillage ne compte pas une seconde fois leur poids et leurs charges (passer en « coque » et retirer les charges linéiques équivalentes pour les calculer, docs/SURFACES.md) ; finitions d'aspect (béton coffré, acier peint, câble…) sans effet sur le calcul.

Contrôles de la suite (à chaque génération) : topologie (doublons, longueurs nulles, nœuds isolés ou non connectés,
structure d'un seul tenant), Σ réactions = Σ q·L sous Q seul, équilibre ELU, contrainte σ = N/A ± M/W (contrôle
analytique), câble bi-encastré N = T0 et haubans tendus, enregistrement et relecture identiques (dont finitions et
tensions), recalcul identique du projet relu, notes de calcul PDF.

Notes de calcul : `notes/<projet>_note_simplifiee.pdf` et `notes/<projet>_note_detaillee.pdf` (18 chapitres), produites
par le module NDC à partir des résultats ELU.

## Dalle calculée en coques (test 356)

`TSA_Exemple_Dalle_Coques.tsa` : dalle de 6 × 4 m (e = 20 cm) sur quatre poutres et quatre poteaux en béton armé, charge
surfacique de 5 kN/m² (cas G, avec poids propre) et maillage enregistré (quadrangles de 0,5 m). Au calcul (F5), la dalle est
transmise en coques OpenSees (96 ShellMITC4) : Σ réactions = 373,1 kN (charge, poids propre des barres et de la dalle),
déplacement maximal de la dalle 1,58 mm. Voir docs/SURFACES.md.

## Voile de réservoir : appui linéique et poussée hydrostatique (test 361)

`TSA_Exemple_Voile_Reservoir.tsa` est un voile de 5 × 3 m (e = 20 cm), encastré sur toute la longueur de son pied par un appui linéique, sous une hauteur d'eau de 2 m (γ = 10 kN/m³). La poussée hydrostatique p = 20 − 10·v (kN/m²) s'applique sur la zone v ≤ 2 m. Le maillage est enregistré (quadrangles de 0,4 m, 104 ShellMITC4).

- **Calcul statique (F5) :** Rz = 73,6 kN (poids propre), poussée de 100 kN appliquée à h/3, déplacement maximal 0,84 mm en tête.
- **Calcul modal** (Ctrl+F5, type « Modale ») : 6 modes, masse 7,5 t, f1 = 12,85 Hz.

## Trois modèles filaires (tests 347 à 349)

| Fichier | Ouvrage | Nœuds | Barres | Appuis |
|---|---|---|---|---|
| `TSA_Demo_Pont_Treillis_3x48m.tsa` | Pont en treillis Warren à montants, 3 travées de 48 m | 253 | 665 | 14 |
| `TSA_Demo_Hangar_60x40m.tsa` | Hangar métallique 60 × 40 m, 11 portiques à fermes | 514 | 1 209 | 27 |
| `TSA_Demo_Batiment_R+5.tsa` | Bâtiment R+5 à ossature métallique | 380 | 798 | 26 |

Barres seulement, calcul OpenSees sous poids propre (cas « G » enregistré dans le projet).

## Régénérer

`TSA_DEMO_DIR=<dépôt>/examples TSA_TestSuite --suite=demo` (projets et notes).
Captures de l'application : `tools/ui_tests/demo_screenshots.ps1` (modèles filaires) et
`tools/ui_tests/showcase_captures.ps1` (quatre projets : rendu, calcul F5, résultats, animation, diagrammes).
