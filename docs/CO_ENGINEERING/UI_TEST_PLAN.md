# Plan de Tests de Bout en Bout de l'Interface Utilisateur (TSA)
**Date :** 19 octobre 2026  
**Statut :** Spécification des Scénarios de Test Système (ISO/IEC/IEEE 29119)  
**Projet :** Tsaraloha Structural Analysis (TSA)  
**Exécutable ciblé :** `build-ninja-release/TSA.exe`

---

## 1. Vue d'Ensemble des Scénarios

Le présent plan de test définit les scénarios complets de bout en bout exécutés de façon autonome par l'Agent 3 (Testeur UI) en liaison avec l'Agent 4 (Expert FEM) et l'Agent 5 (Auditeur Qualité).

```text
┌────────────────────────────────────────────────────────────────────────┐
│                        SUITES DE TESTS UI SYSTÈME                      │
├─────────────┬─────────────────────────────────────┬────────────────────┤
│ Identifiant │ Scénario utilisateur testé          │ Type d'essai       │
├─────────────┼─────────────────────────────────────┼────────────────────┤
│ SC-A        │ Démarrage & Gestion des Projets     │ Cycle de vie       │
│ SC-B        │ Modélisation Portique 2D            │ Géométrie & RDM    │
│ SC-C        │ Résolution OpenSees & Résultats     │ Validation FEM     │
│ SC-D        │ Robustesse, Redimensionnement & Undo│ Résilience UI      │
└─────────────┴─────────────────────────────────────┴────────────────────┘
```

---

## 2. Définition Détaillée des Scénarios

### Scénario SC-A : Démarrage & Gestion des Projets
- **SC-A.1 : Démarrage propre sans crash**
  - *Action :* Lancement de `TSA.exe` avec variables d'environnement configurées (Qt 6, OCCT, DLLs 3rdparty).
  - *Vérification :* Processus actif, création du canal IPC `tsaraloha-tsa`, apparition de la fenêtre principale avec titre conforme en moins de 5 secondes.
  - *Preuve :* Capture d'écran `tsa_startup.png`.
- **SC-A.2 : Création de projet vierge**
  - *Action :* Déclenchement de la commande de nouveau projet.
  - *Vérification :* Modèle initialisé à 0 nœud, 0 barre, arbre du modèle réinitialisé.
- **SC-A.3 : Isolation multi-documents**
  - *Action :* Création d'un second document / onglet.
  - *Vérification :* Le serveur d'automatisation bascule sur l'identifiant du nouvel onglet actif. Les modifications du document 2 n'altèrent pas le document 1.

---

### Scénario SC-B : Modélisation Structurelle Complète (Portique 2D)
- **SC-B.1 : Implantation des nœuds**
  - *Nœuds créés :*
    - $N1 : (0{,}0, 0{,}0, 0{,}0)\,\text{m}$ (Pied poteau gauche)
    - $N2 : (0{,}0, 0{,}0, 3{,}5)\,\text{m}$ (Tête poteau gauche / Angle gauche)
    - $N3 : (6{,}0, 0{,}0, 3{,}5)\,\text{m}$ (Tête poteau droit / Angle droit)
    - $N4 : (6{,}0, 0{,}0, 0{,}0)\,\text{m}$ (Pied poteau droit)
  - *Vérification :* 4 nœuds enregistrés avec coordonnées conformes.
- **SC-B.2 : Assignation des sections et matériaux**
  - *Éléments créés :*
    - Poteau gauche $C1$ reliant $N1 \to N2$ : Profilé `HEB 200`, Matériau acier `S235` ($E = 210\,\text{GPa}$).
    - Poteau droit $C2$ reliant $N4 \to N3$ : Profilé `HEB 200`, Matériau acier `S235` ($E = 210\,\text{GPa}$).
    - Poutre traverse $B1$ reliant $N2 \to N3$ : Profilé `IPE 300`, Matériau acier `S235` ($E = 210\,\text{GPa}$).
  - *Vérification :* 2 poteaux et 1 poutre enregistrés avec leurs caractéristiques d'inertie ($I_y, I_z, A$).
- **SC-B.3 : Conditions aux limites (Appuis)**
  - *Action :* Encastrement parfait (`fixed`) aux nœuds de pied $N1$ et $N4$ (blocage des 6 DDLs).
  - *Vérification :* $N1$ et $N4$ déclarés `supportedNodes`, DDLs de translation et rotation verrouillés.
- **SC-B.4 : Définition des charges**
  - *Action :*
    - Création du cas de charge permanent $G$ (catégorie `dead`).
    - Application d'une charge linéique descendante uniforme $q = 15{,}0\,\text{kN/m}$ sur la traverse $B1$.
  - *Vérification :* 1 cas de charge, 1 charge répartie associée à $B1$.
  - *Preuve :* Capture d'écran `tsa_modeled.png`.

---

### Scénario SC-C : Résolution OpenSees & Confrontation Analytique
- **SC-C.1 : Déclenchement du calcul**
  - *Action :* Appel de la commande `analysis.run` (F5 / Ruban Analyse).
  - *Vérification :* Calcul asynchrone achevé sans blocage du thread UI, statut `success: true`.
- **SC-C.2 : Extraction et vérification des résultats**
  - *Extraction via `results.summary` :*
    - Déplacement vertical maximal en travée de la traverse $B1$.
    - Moment fléchissant maximal à mi-travée $M_y$.
    - Réactions verticales d'appui aux pieds $N1$ et $N4$.
- **SC-C.3 : Confrontation avec la référence indépendante (Agent 4 FEM)**
  - *Valeurs analytiques théoriques de référence (RDM) :*
    - Réaction verticale totale : $R_{tot} = q \times L = 15{,}0 \times 6{,}0 = 90{,}0\,\text{kN}$.
    - Réaction par appui (symétrie) : $R_{1z} = R_{4z} = 45{,}0\,\text{kN}$.
    - Moment fléchissant approché à mi-travée : ordre de grandeur $45$ à $55\,\text{kN}\cdot\text{m}$ (tenant compte du semi-encastrement élastique apporté par les poteaux HEB 200).
  - *Critère de succès :* Écart numérique entre TSA/OpenSees et le calcul indépendant $< 1{,}0\%$.
  - *Preuve :* Capture d'écran `tsa_results.png`.

---

### Scénario SC-D : Robustesse, Redimensionnement & Annulation
- **SC-D.1 : Annulation / Rétablissement (Undo/Redo)**
  - *Action :* Exécution successive de `undo` puis `redo`.
  - *Vérification :* Restauration exacte de l'état du modèle, invalidation propre des résultats au moment de l'annulation.
- **SC-D.2 : Redimensionnement de fenêtre & Responsive UI**
  - *Action :* Redimensionnement de la fenêtre principale (1024x768, 1600x900, 1920x1080).
  - *Vérification :* Aucun widget masqué ou écrasé, le viewport 3D OCCT s'adapte sans scintillement.
- **SC-D.3 : Fermeture propre de l'exécutable**
  - *Action :* Envoi de l'événement de fermeture WM_CLOSE ou `app.quit`.
  - *Vérification :* Fermeture propre du process `TSA.exe`, libération des ressources sans processus fantôme.
