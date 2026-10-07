# Système d'Analyse Structurelle OpenSees — TSA

> Document conforme à la norme **IEEE Std 1063-2001 (R2007)** et **ISO/IEC/IEEE 26514:2022**.
> Référence technique pour le module `src/Analysis` et son intégration avec le modèle structural TSA.

---

## 1. Vue d'Ensemble & Principes Architecturaux

Le module `src/Analysis` assure l'intégration professionnelle et bidirectionnelle du moteur de calcul open-source **OpenSees** (Open System for Earthquake Engineering Simulation) dans TSA.

### 1.1 Source Unique de Vérité & Immutabilité (Snapshot)

1. **Source de Vérité Unique (SSOT)** : `TSA::Model::Model` est le modèle métier souverain.
2. **Immutabilité des Données en Calcul** : Aucune analyse n'interagit directement avec le modèle actif en cours d'édition.
   - À chaque lancement de calcul, une copie figée `TSA::Analysis::CalculationSnapshot` est extraite du modèle.
   - Les modifications ou sélections intervenant dans l'UI durant le calcul n'affectent pas la simulation.
3. **Exécution Asynchrone & Non-Bloquante** :
   - `TSA::Analysis::OpenSeesSolver` encapsule l'exécution de `OpenSees.exe` dans un processus isolé (`QProcess`) exécuté en arrière-plan.
   - L'interface utilisateur reste fluide à 60 FPS, avec possibilité d'annulation immédiate (`killProcess`).
4. **Zéro Chemin Codé en Dur** :
   - `TSA::Analysis::OpenSeesManager` gère la détection automatique locale, la validation d'intégrité, la configuration de chemin personnalisé et le téléchargement automatisé sécurisé des binaires officiels de Berkeley si requis.

---

## 2. Pipeline de Calcul

```text
TSA::Model::Model
  │  (Elements, Nodes, Loads, LoadCases, Combinations)
  ▼
TSA::Analysis::CalculationSnapshot
  │  (Snapshot immuable, DOFs, validation de connectivité)
  ▼
TSA::Analysis::OpenSeesAnalysisBuilder
  │  (Génération du script Tcl paramétrique avec commandes OpenSees standard)
  ▼
TSA::Analysis::OpenSeesSolver
  │  (Exécution du sous-processus OpenSees.exe, redirection stdout/stderr)
  ▼
TSA::Analysis::OpenSeesResultsReader
  │  (Lecture des enregistreurs nodaux, calcul isostatique aux stations)
  ▼
TSA::Analysis::ResultsModel
  │  (Déplacements nodaux, efforts N/Vy/Vz/Mx/My/Mz en convention RDM, déformée intégrée)
  ├──► Affichage 3D (src/Viewer/ResultsVisualManager)
  ├──► Diagrammes 2D (src/UI/Diagrams/Diagram2DWidget)
  └──► Note de Calcul (src/NDC/NDCGenerator)
```

---

## 3. Composants du Module `src/Analysis`

| Classe / Fichier | Responsabilité |
|---|---|
| `OpenSeesManager` | Détection locale du binaire `OpenSees.exe`, validation de version, gestion des chemins utilisateur et téléchargement de secours. |
| `CalculationSnapshot` | Capture figée et optimisée du modèle structural (`NodeSnapshot`, `ElementSnapshot`, `LoadSnapshot`), mapping des degrés de liberté (DOFs). |
| `OpenSeesAnalysisBuilder` | Générateur de scripts Tcl selon le type d'analyse : Linéaire statique, Non-linéaire statique (P-Delta, corotationnel). Charges trapézoïdales par forces d'encastrement parfait (OpenSees 3.8 ne gère en 3D que la charge uniforme partielle). |
| `OpenSeesSolver` | Exécution asynchrone non-bloquante du processus OpenSees, streaming des logs et gestion du cycle de vie (démarrage, arrêt, timeout). |
| `OpenSeesResultsReader` | Analyse et dé-sérialisation des fichiers de sortie OpenSees (recorders), intégration des efforts tranchants et moments fléchissants locaux. |
| `ResultsModel` | Conteneur des résultats d'analyse : déplacements $\mathbf{U}$, réactions aux appuis $\mathbf{R}$, efforts internes aux stations (convention RDM documentée sur `StationForces`), bilan d'équilibre forces ET moments. |

---

## 4. Types d'Analyses Prises en Charge

### 4.1 Analyse Statique Linéaire (1er Ordre)
- Résolution par éléments finis standards 3D (éléments poutres d'Euler-Bernoulli ou de Timoshenko).
- Superposition des cas de charges simples ou combinaisons Eurocodes (ELU / ELS).
- Calcul des efforts intérieurs le long de 11 à 21 stations par membre avec prise en compte isostatique des charges réparties directes sur élément (`eleLoad -beamUniform` et `-beamPoint`).

### 4.2 Analyse Non-Linéaire Statique
- Transformation géométrique `Corotational` ou `PDelta`, pilotage `LoadControl` / `DisplacementControl` / `ArcLength`.
- Facteur de charge λ de chaque pas lu et appliqué aux efforts des stations.

### 4.3 Hors périmètre
Modal, pushover et temporel ont été retirés (ADR-022) : la version qui les contenait est figée sur
la branche `archive/dynamique` (tag `v0.2.0-dynamique`).

---

## 5. Validation Analytique & Tests Régression

Le module d'analyse est validé par une batterie de tests analytiques continus (`tests/test_opensees.cpp`) :

1. **Poutre Simple sous Charge Ponctuelle Centrale** :
   - Moment fléchissant théorique : $M_{\max} = \frac{P L}{4}$.
   - Pour $L = 5\text{ m}$, $P = 10\text{ kN}$ : $M_{\max} = 12.5\text{ kNm}$. Erreur relative $< 0.1\%$.
2. **Poutre sous Charge Uniformément Répartie** :
   - Réaction aux appuis théorique : $R_y = \frac{q L}{2}$.
   - Pour $L = 6\text{ m}$, $q = 20\text{ kN/m}$ : $R = 60\text{ kN}$. Erreur relative $< 0.01\%$.
3. **Vérification de l'Équilibre Statique Global** :
   - $\sum F_x = 0$, $\sum F_y = 0$, $\sum F_z = 0$ contrôlés automatiquement dans `ResultsModel::checkEquilibrium()`.
4. **Poutres hyperstatiques, charges trapézoïdales, rotules et treillis** (`tests/test_opensees_extraction.cpp`, tests 181 à 184) :
   - encastrée–appuyée, console sous charge triangulaire, rotule `-releasez`, trépied de treillis ; équilibre des moments contrôlé.
