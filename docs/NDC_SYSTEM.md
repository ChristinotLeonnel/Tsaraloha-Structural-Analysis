# Système de Note de Calcul (NDC) — TSA

> Document conforme à la norme **IEEE Std 1063-2001 (R2007)**, **ISO/IEC/IEEE 26514:2022** et aux **Eurocodes (EN 1990 à EN 1999)**.
> Référence technique pour le module `src/NDC`.

---

## 1. Vue d'Ensemble

Le module `src/NDC` génère automatiquement une **Note de Calcul d'Ingénierie Structurelle** complète, certifiable et auditable à partir du modèle structural TSA et des résultats d'analyse OpenSees.

### 1.1 Normes Appliquées
- **IEEE Std 1063-2001 (R2007)** : Structure en 5 sections normalisées, identification complète du document, clarté, traçabilité et exhaustivité des données.
- **Eurocodes Structuraux** :
  - **EN 1990 (Eurocode 0)** : Bases de calcul des structures (combinaisons d'actions ELU / ELS).
  - **EN 1991 (Eurocode 1)** : Actions sur les structures (poids propre, charges d'exploitation, neige, vent).
  - **EN 1992 (Eurocode 2)** : Calcul des structures en béton armé.
  - **EN 1993 (Eurocode 3)** : Calcul des structures en acier.
  - **EN 1998 (Eurocode 8)** : Calcul des structures pour leur résistance aux séismes.

---

## 2. Architecture du Module `src/NDC`

```text
src/NDC/
├── NDCDocumentModel.h/.cpp   — Modèle objet du document (Chapitres, Sections, Tableaux, Clé-Valeurs)
├── NDCGenerator.h/.cpp       — Moteur de génération des 9 chapitres techniques à partir du Model et de ResultsModel
├── NDCExporter.h/.cpp        — Exportateurs de format (HTML moderne avec styles CSS imprimables, PDF vectoriel)
└── NDCViewerWidget.h/.cpp    — Visualiseur interactif intégré (QSplitter, sommaire arborescent QTreeWidget, QWebEngine/QTextBrowser)
```

---

## 3. Structure des Chapitres Générés

Le `NDCGenerator` assemble automatiquement un rapport composé de 9 chapitres rigoureusement structurés :

1. **Chapitre 1 : Identification du Projet & Cadre Normatif**
   - Nom de l'ouvrage, date, révision, ingénieur responsable, version de TSA et d'OpenSees.
   - Liste des normes Eurocodes applicables et système d'unités (SI : mètres, kN, kNm, MPa).
2. **Chapitre 2 : Hypothèses Générales & Description du Système Structural**
   - Typologie structurale (portiques spatiaux 3D, treillis, câbles, dalles).
   - Conditions de site et critères de flèche admissible (ex. $L/250$, $L/350$).
3. **Chapitre 3 : Caractéristiques des Matériaux & Sections Transversales**
   - Matériaux : Modules d'élasticité $E$, coefficients de Poisson $\nu$, densités volumiques $\rho$, limites élastiques $f_y$ ou résistances caractéristiques $f_{ck}$.
   - Sections : Dimensions géométriques, aires $A$, inerties de flexion $I_y, I_z$, inertie de torsion $I_t$.
4. **Chapitre 4 : Géométrie Nodal & Conditions aux Limites (Appuis)**
   - Tableau récapitulatif des coordonnées de l'ensemble des nœuds ($X, Y, Z$).
   - Conditions de fixation des degrés de liberté (Encastrements, rotules, appuis simples, ressorts).
5. **Chapitre 5 : Inventaire des Charges & Combinaisons d'Actions**
   - Cas de charges permanentes ($G$), variables d'exploitation ($Q$), climatiques ($W, S$).
   - Définition des combinaisons fondamentales ELU ($\sum \gamma_{G,j} G_{k,j} + \gamma_Q Q_{k,1} + \dots$) et ELS.
6. **Chapitre 6 : Réactions d'Appuis & Vérification de l'Équilibre Global**
   - Bilan des réactions nodales $(R_x, R_y, R_z, M_x, M_y, M_z)$.
   - Contrôle strict de l'équilibre statique global : validation de $\sum F_{ext} + \sum R = 0$.
7. **Chapitre 7 : Déplacements Nodaux & Vérification des Flèches**
   - Déplacements maximaux sous combinaisons de service ELS.
   - Comparaison avec les flèches admissibles réglementaires (statut Conforme / Non-conforme).
8. **Chapitre 8 : Enveloppe des Sollicitations Internes**
   - Tableau synthétique des efforts normaux extrêmes ($N_{\max}, N_{\min}$), efforts tranchants ($V_{\max}$) et moments fléchissants ($M_{\max}$) par élément.
9. **Chapitre 9 : Analyse Dynamique & Modes Propres (si calculé)**
   - Fréquences propres $f_i$ (Hz), périodes $T_i$ (s), pulsations $\omega_i$ (rad/s) et pourcentages de masses modales participantes.

---

## 4. Visualisation & Export

- **Visualiseur Intégré (`NDCViewerWidget`)** :
  - Intégré dans l'espace de travail TSA via le système multi-ports ou comme panneau dockable.
  - Sommaire interactif synchronisé : cliquer sur un chapitre ou sous-titre scrolle instantanément la vue.
  - Recherche textuelle en direct avec surlignage.
- **Export Multi-Format (`NDCExporter`)** :
  - **HTML 5 / CSS 3** : Rendu visuel soigné, support du mode sombre et mise en page optimisée pour l'impression `@media print`.
  - **PDF Vectoriel** : Export direct via `QPdfWriter` / `QPrinter` avec numérotation des pages et en-tête d'ingénierie.
