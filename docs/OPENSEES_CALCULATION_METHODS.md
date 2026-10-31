# Méthodes de Calcul Structurel — TSA & OpenSees

> **Guide technique pour les utilisateurs et ingénieurs**
> 
> Ce document explique les **algorithmes, solveurs et types d'analyse** qu'utilise TSA avec OpenSees pour la simulation structurelle 3D.
>
> **TSA prend actuellement en charge l'analyse structurale linéaire. L'analyse non linéaire n'est pas disponible dans cette version.**

---

## 1. Types d'Analyses Disponibles

### 1.1 Analyse Statique Linéaire (1er Ordre)

**Description** : Calcul des déplacements, réactions et efforts sous l'hypothèse de petits déplacements et matériau linéaire élastique.

**Paramètres de calcul** :
- **Algorithme** : `Linear` (direct, non itératif : une seule factorisation)
- **Intégrateur** : `LoadControl 1.0` (charge appliquée en une seule étape complète, λ = 1)
- **Résolution** : un seul pas (`analyze 1`)
- **Test** : `NormDispIncr 1e-06 50`, exigé par la commande `analysis` d'OpenSees ; non évalué par `algorithm Linear`
- **Système solver** : `BandGeneral` (par défaut)
- **Transformation géométrique** : `geomTransf Linear` pour toutes les poutres et tous les poteaux

**Cas d'usage** :
- Structures de bâtiment sous charges permanentes et d'exploitation
- Combinaisons Eurocodes (ELU / ELS)
- Superposition linéaire de cas de charge

**Exemple** :
```
Poutre simple, L = 5 m, P = 10 kN au centre
→ M_max = PL/4 = 12.5 kNm (théorique)
→ Erreur dans TSA : < 0.1 %
```

---

### 1.2 Analyse Non Linéaire : non disponible

L'analyse non linéaire n'est pas disponible dans cette version (ADR-025). TSA ne génère ni algorithme itératif (Newton
et variantes), ni pilotage incrémental (`DisplacementControl`, `ArcLength`, `MinUnbalDispNorm`, plusieurs pas de
`LoadControl`), ni transformation `PDelta` ou `Corotational`.

Un projet enregistré par une version antérieure qui demandait une analyse statique non linéaire, une transformation
P-Delta ou corotationnelle, ou des treillis corotationnels, s'ouvre normalement : ses autres données sont conservées.
Le calcul est refusé avec un message clair. Il n'est jamais recalculé en linéaire à l'insu de l'utilisateur. Choisir une
analyse linéaire dans les paramètres d'analyse lève le refus.

Les défauts BUG-149 (facteur de charge des efforts le long des barres en non linéaire) et BUG-150 (P-Delta accepté sans
effet en statique linéaire) ont disparu avec la fonctionnalité : voir `docs/audit/2026-10-31/TSA_BUG_REGISTER.md`.

---

### 1.3 Analyse Modale

**Description** : Calcul des fréquences propres et des formes modales de vibration.

**Paramètres** :
- **Solveur eigenvalue** : 
  - LAPACK (dense) si DDL libres ≤ 3000
  - ARPACK (creux) sinon
- **Nombre de modes** : jusqu'à (DDL_libres − 1)
- **Masses** : 
  - Masses de volume : ρ·A pour les barres, ρ·h pour les coques
  - Masses ajoutées : m = Fz_descendante / g (9.81 m/s²)

**Sortie** :
- Valeurs propres λ (rad²/s²) → ω = √λ, f = ω/(2π)
- Vecteurs propres (déformée normalisée)
- Propriétés modales : masses modales, participation, facteurs d'amortissement (0 % — structure non amortie)

---

## 2. Éléments Structuraux & Lois Constitutives

### 2.1 Poutres et Poteaux

| Propriété | Valeur |
|---|---|
| **Type élément OpenSees** | `elasticBeamColumn` (3D) |
| **Théorie de poutre** | Euler-Bernoulli (6 DDL par nœud : Ux, Uy, Uz, Rx, Ry, Rz) |
| **Loi matériau** | Élastique linéaire (E, G constants) |
| **Formulation** | Transformation géométrique `Linear` (petits déplacements) |
| **Torsion** | Saint-Venant (GI_p) |
| **Flexion** | Biaxiale (EI_y, EI_z) |

**Section transversale** : Rectangle, Cercle, Profil, Section composite (I, C, T, L, etc.)

### 2.2 Treillis et Câbles

| Propriété | Treillis | Câble |
|---|---|---|
| **Type** | `truss` | `corotTruss` (élément historique, conservé pour ne pas changer des résultats validés) |
| **Chargement** | Effort axial N uniquement | Effort axial N uniquement ; précontrainte par `InitStrain` (ε₀ = T₀ / EA) |
| **DDL bloqués** | Rx, Ry, Rz (articulations) | Rx, Ry, Rz (articulations) |
| **Résolution** | Un seul pas linéaire | Un seul pas linéaire à la géométrie initiale, sans itération ; une compression n'est pas relâchée (BUG-151) |

---

## 3. Gestion des Charges

### 3.1 Charges sur Barres (Poutres/Poteaux)

| Type | Commande OpenSees | Répère Local | Exemple |
|---|---|---|---|
| **Uniforme** | `eleLoad -beamUniform Wy Wz Wx` | Oui | q = 20 kN/m sur toute la longueur |
| **Ponctuelle** | `eleLoad -beamPoint Py Pz xL Px` | Oui | P = 50 kN à x = L/2 |
| **Trapézoïdale** | Forces nodales équivalentes | Oui | q₁ = 10, q₂ = 20 kN/m sur [a, b] |
| **Moment réparti** | Forces nodales équivalentes | Oui | m = 5 kNm/m (intégration Gauss 4 pts) |

**Calcul des forces d'encastrement parfait** (pour charges linéaires et couples) :
- Intégration numérique Gauss-Legendre 4 points (exacte pour degrés 1–4)
- Les forces équivalentes sont appliquées aux nœuds pour reconstituer la charge distribuée

### 3.2 Charges Nodales

| Type | Composantes | Repère |
|---|---|---|
| **Forces ponctuelles** | Fx, Fy, Fz (kN) | Global |
| **Moments nodaux** | Mx, My, Mz (kNm) | Global |

### 3.3 Charges Surfaciques (Dalles, Voiles)

- **Source** : Maillage FEM (Gmsh)
- **Transmission** : Forces nodales équivalentes Galerkin (∫ Nᵢ p dA)
- **Poids propre** : Densité × volume / nombre de nœuds

### 3.4 Poids Propre

**Automatique** :
- Poutres/poteaux : ρ·A·g (décomposé en repère local)
- Treillis/câbles : charge répartie → nœuds (règle du levier)
- Coques : ρ·h intégré au maillage

---

## 4. Solveurs Linéaires et Numérotation

### 4.1 Numérotation des DDL

| Algorithme | Description | Performance |
|---|---|---|
| **RCM** | Reverse Cuthill-McKee | Réduit la bande → BandGeneral rapide |
| **Plain** | Numérotation de base | Sans optimisation |

**Impact** : Réduction de 40–60 % du temps de factorisation (matrices bande).

### 4.2 Solveurs Disponibles

| Solveur | Type | Taille | Avantages |
|---|---|---|---|
| **BandGeneral** | Bande dense | Petit/moyen (< 5000 DDL) | Rapide, mémoire prévisible |
| **ProfileSPD** | Profil symétrique | Petit/moyen | Symétrique défini positif |
| **BandSPD** | Bande symétrique | Petit/moyen | SDP seulement |
| **SuperLU** | Creux direct (LU) | Moyen/grand (> 5000 DDL) | Remplis faible, très rapide |
| **UmfPack** | Creux direct (LU) | Moyen/grand | Alternative SuperLU, stable |
| **SparseGEN** | Creux itératif | Très grand | Mémoire réduite |

**Recommandation** : 
- Petit bâtiment : `BandGeneral` (défaut)
- Grand bâtiment / pont : `SuperLU`
- Très grand système : `SparseGEN` (itératif, moins précis)

### 4.3 Gestion des Contraintes (Appuis)

| Handler | Approche | Usage |
|---|---|---|
| **Transformation** | Élimination des DDL fixés (défaut) | Standard, performant |
| **Plain** | Contraintes directes (simples) | Cas simple |
| **Penalty** | Pénalités 1e12 (Kc·1e12) | Moins stable |
| **Lagrange** | Multiplicateurs de Lagrange | Correct, moins efficace |

**Appuis élastiques** (ressorts) :
- Nœud auxiliaire entièrement fixé
- Élément `zeroLength` avec `uniaxialMaterial Elastic` par DDL
- Raideur par direction (Kx, Ky, Kz, Krx, Kry, Krz)

---

## 5. Précision Numérique et Extraction

### 5.1 Enregistrement des Résultats

| Sortie | Contenu | Précision |
|---|---|---|
| **Déplacements** | UX, UY, UZ, RX, RY, RZ (6 DDL) | 16 chiffres |
| **Réactions** | Forces Fx, Fy, Fz, Moments Mx, My, Mz | 16 chiffres |
| **Efforts barres** | N, Vy, Vz, T, My, Mz (repère local, 12 val.) | 16 chiffres |
| **Efforts treillis** | Effort axial N (traction > 0) | 16 chiffres |
| **Coques** | Résultantes (Nxx, Nyy, Nxy, Mxx, Myy, Mxy, Vxz, Vyz) | 16 chiffres |

### 5.2 Validation de l'Équilibre

**Automatique** (`ResultsModel::checkEquilibrium()`) :
- Somme des forces : ΣFx = 0, ΣFy = 0, ΣFz = 0
- Somme des moments : ΣMx = 0, ΣMy = 0, ΣMz = 0
- Tolérance : 1e-6 × force max

### 5.3 Extraction Avancée (Mode Advanced)

**Disponible si** : DDL libres ≤ 1500 (seuil configurable)

| Matrice | Contenu | Usage |
|---|---|---|
| **K_global** | Rigidité globale (n × n dense) | Analyse fréquentielle, sensibilité |
| **K_basic_beam** | Rigidité basique barre (6 × 6) | Propriétés géométriques |
| **K_basic_truss** | Rigidité basique treillis (2 × 2) | Propriétés axiales |
| **DOF_map** | Correspondance nœud → DDL global | Reconstruction locale-globale |

---

## 6. Benchmarks & Validation

### 6.1 Cas Analytiques de Référence

| Cas | Configuration | Résultat TSA | Erreur |
|---|---|---|---|
| **Poutre simple** | L=5m, P=10kN centre | M_max = 12.5 kNm | < 0.1 % |
| **Poutre encastrée** | L=6m, q=20kN/m | R = 60 kN | < 0.01 % |
| **Équilibre global** | ΣF_x, ΣF_y, ΣF_z, ΣM_x, ΣM_y, ΣM_z | Zéro | < 1e-6 |
| **Treillis trépied** | Effort axial de traction | Équilibre nodal | < 0.1 % |

Voir `tests/test_opensees.cpp` et `tests/test_opensees_extraction.cpp` (tests 181–184).

---

## 7. Limitations & Hypothèses

### 7.1 Hypothèses Structurales

✓ **Inclus** :
- Petits déplacements, premier ordre (analyse linéaire)
- Matériaux isotropes homogènes élastiques
- Sections constantes par élément
- Absence d'amortissement (analyse statique)

✗ **Non inclus** (hors de portée) :
- Analyse non linéaire : second ordre P-Δ, grands déplacements, pilotage incrémental
- Plasticité, fissuration béton
- Fatigue, cumul d'endommagement
- Phénomènes dynamiques (sisme, vent temporel)
- Contact / choc
- Flambement local de section

### 7.2 Limitations OpenSees 3.8.0

| Limitation | Contournement |
|---|---|
| Charge 3D trapézoïdale non gérée par `eleLoad` | Forces nodales équivalentes (exactes) |
| Moment réparti non géré | Forces nodales équivalentes (intégration Gauss) |
| CorotTruss sans `localForce` | `basicForce` utilisé (effort axial) |

---

## 8. Flux Complet de Calcul

```
Modèle TSA
│
├─ Capture figée (CalculationSnapshot)
│  ├─ Nœuds, éléments
│  ├─ Appuis, ressorts
│  ├─ Charges (nodales, barres, surfaciques)
│  └─ DOF mapping
│
├─ Générateur Tcl (OpenSeesAnalysisBuilder)
│  ├─ Nœuds: node $id $x $y $z
│  ├─ Conditions limites: fix, spring (zeroLength)
│  ├─ Éléments: elasticBeamColumn, Truss
│  ├─ Charges: eleLoad, load, pattern Plain
│  └─ Résolution: constraints, numberer, system, algorithm, integrator
│
├─ Exécution (OpenSeesSolver)
│  └─ Subprocess: OpenSees.exe < script.tcl
│
├─ Extraction (OpenSeesResultsReader)
│  ├─ Déplacements nodaux
│  ├─ Réactions
│  ├─ Efforts locaux (stations)
│  └─ Validation équilibre
│
└─ Résultats (ResultsModel)
   ├─ Affichage 3D (déformée, heatmap d'effort)
   ├─ Diagrammes 2D
   └─ Note de Calcul (NDC)
```

---

## 9. Pour Aller Plus Loin

- **OpenSees Documentation** : https://opensees.berkeley.edu/wiki/index.php/Command_Manual
- **Théorie FEM 3D** : Voir `docs/ANALYTICAL_MODEL.md`
- **Tests de régression** : `tests/test_opensees.cpp` et `test_opensees_extraction.cpp`
- **Configuration avancée** : `src/Analysis/OpenSeesAnalysisBuilder.h` (AnalysisParameters)

---

**Mis à jour** : 2026-10-31 (analyse non linéaire retirée, ADR-025) | **Version TSA** : 3.0+ avec OpenSees 3.8.0

