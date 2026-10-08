# Architecture de l'écosystème Tsaraloha — TSA et TSALab

Statut : décision ADR-024 (2026-10-08), remplace la mise en œuvre « TSALab = enveloppe de MainWindow » d'ADR-023.
Ce document est la référence pour tout développement touchant aux deux applications.

## 1. Constat (analyse du code de TSA, 2026-10-08)

Graphe des dépendances entre dossiers de `src/` (inclusions réelles) :

| Couche constatée | Dossiers | Widgets Qt |
| :--- | :--- | :--- |
| Modèle de projet | Model, Coordinate, Grid (logique), BIM, ExtensionSystem, Library, Commands, UndoRedo, IO, Project, Diagnostics, Standards, Analysis, Geometry (B-Rep OCCT), NDC (génération), AI (cœur) | aucun |
| Graphique | Viewer (OccView, SelectionManager, Projection, ResultsVisual…), Grid (rendu, accrochage), Interaction (outils), UI/Theme, UI/Ruler | OccView seul (QWidget du viewport) |
| Application TSA | UI (ruban, docks, dialogues, Start Center, AppShell), NDC (widgets) | 106 fichiers |

- Le viewport `OccView` **ne dépend pas de MainWindow**. Il dépend du modèle, des grilles, de la géométrie et des
  outils. Seules 3 inclusions franchissaient une couche : `ThemeManager` (aucune dépendance, devient fondation
  graphique) et `NDC/ResultAnalyzer` (logique pure, couche modèle).
- `MainWindow` est l'unique endroit où sont assemblés modèle, CommandManager, SelectionManager, GridManager,
  ProjectManager, gardien de résultats, registre de moteurs : c'est ce câblage qui empêche une 2ᵉ application
  de réutiliser le viewport sans copier MainWindow.
- Le calcul (Custom2D / MetDeDeplacement) est déjà derrière un contrat neutre (`Custom2DSolver.h`, bibliothèque
  `MetDeDeplacement` sans Qt). OpenSees : générateur et lecteur sans Qt, mais construits sur le modèle TSA.

## 2. Options comparées

| Critère | A. Bibliothèque graphique seule | B. Noyau de scène + renderers par application | C. Modèle + interaction + rendu partagés (retenue) |
| :--- | :--- | :--- | :--- |
| Duplication | modèle et commandes dupliqués ou re-couplés | renderers dupliqués (2 façons de dessiner une poutre) | aucune : un objet, une représentation, une commande |
| Couplage | le graphique devrait connaître un modèle abstrait à inventer | faible mais au prix d'une abstraction de scène générique à écrire | explicite, par couches contrôlées |
| Effort / risque | réécrire OccView sur une abstraction | réécrire tout le rendu | découper le build, extraire la session de projet |
| Fidélité au code | non (OccView manipule le modèle directement) | non | oui (c'est déjà la structure réelle) |
| UI différentes | oui | oui | oui (seules les couches basses sont partagées) |

**A et B obligent à écrire une abstraction de scène générique alors qu'un seul modèle de données doit exister
(§34 de la mission) : elles déplacent la duplication au lieu de la supprimer.** C formalise ce qui existe.

## 3. Architecture retenue

```text
 TSALab (dépôt TSALab)                     TSA (dépôt TSA)
 ├─ app TSALab.exe : IDE scientifique      ├─ app TSA.exe : ruban, AppShell, Start Center, docks
 │  workspaces Model / Blueprint /         │
 │  Analysis / Results / Research          │
 ├─ tsalab_blueprint (runtime, sans Qt)    │
 └─ tsalab_science  (sans Qt, sans OCCT) ◄─┼──── utilisé par TSA (moteurs de calcul)
      numerics, moteurs (Custom2D, FEM,    │
      OpenSees), validation, benchmarks    │
                 ▲                         │
                 └──── utilisent ──────────┤
          ┌──────────────────────────────────────────────────────┐
          │ Bibliothèques partagées (sources dans TSA/src)       │
          │  tsaraloha_graphics : OccView, sélection, picking,   │
          │     caméra/projection, grilles (rendu), accrochage,  │
          │     outils d'interaction, thème, règles du viewport  │
          │  tsaraloha_model    : modèle de projet, géométrie,   │
          │     commandes, undo/redo, IO .tsa, ProjectSession,   │
          │     orchestration d'analyse, résultats, NDC, IA      │
          └──────────────────────────────────────────────────────┘
```

Règles (vérifiées automatiquement par `tools/check_layers.py`, exécuté par les tests) :
1. `tsaraloha_model` n'inclut ni Viewer, ni Interaction, ni UI, ni aucun widget Qt.
2. `tsaraloha_graphics` n'inclut pas l'UI d'une application (UI/ sauf Theme et Ruler).
3. `tsalab_science` n'inclut ni Qt, ni OpenCASCADE, ni le modèle TSA : il reçoit un modèle d'analyse neutre.
4. Une application ne copie jamais un fichier d'une bibliothèque : elle ajoute un point d'extension.
5. Toute commande métier est implémentée une fois (registre de commandes) et appelée par l'UI, le Blueprint,
   l'IA et la console.

Les bibliothèques restent dans `TSA/src` (pas de déplacement de fichiers : historique et inclusions préservés) ;
leur périmètre est défini dans `cmake/TSAProduct.cmake`. Les deux dépôts sont clonés côte à côte.

## 4. Feuille de route (chaque étape : compilation des deux applications + tests)

| Phase | Contenu | Étapes de la mission |
| :--- | :--- | :--- |
| 1 | Bibliothèques `tsaraloha_model` / `tsaraloha_graphics` explicites, contrôle des couches  — **fait**| 1–6 |
| 2 | `ProjectSession` : assemblage modèle/commandes/sélection/grilles/projet hors de MainWindow  — **fait**| 6 |
| 3 | TSALab.exe : fenêtre IDE propre (docks, workspace Model sur le viewport partagé)  — **fait**| 7–8, 12 |
| 4 | `tsalab_science` : numerics, Custom2D/MetDeDeplacement déplacés, validation ; TSA l'utilise  — **fait**| 13 |
| 5 | Registre de commandes exécutables partagé (UI, Blueprint, IA, console)  — **fait**| 9 |
| 6 | Blueprint : runtime typé (données / exécution), sérialisation `.tsbp`, éditeur — **fait** (docs/BLUEPRINT.md) | 10–11, 16–21 |
| 7 | AnalysisController (session), Analysis Manager partagé, système K·U = F (matrix viewer = dock Données d'analyse), espaces Analyse / Résultats / Recherche de TSALab — **fait** | 14–15 |
| 8 | Débogueur, profileur, API de plugins, IA sur les Blueprints, OpenSees dans tsalab_science | 16–18 |
