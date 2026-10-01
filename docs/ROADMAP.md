# Roadmap — TSA

> Document vivant. Ce fichier fait partie du framework `.agents/` mis en place pour les
> agents IA — il ne remplace aucune feuille de route existante par ailleurs.

## État constaté à la mise en place du framework agents

- Suite de tests `TSA_Tests` : 48/48 PASS (voir `README.md`).
- Documentation utilisateur/technique existante : `DOCUMENTATION.md`, `README.md`.
- Documentation technique existante par sujet : `docs/TSALIB_SYSTEM.md`,
  `docs/TSA_DIAGNOSTICS.md`, `docs/TSA_FILE_FORMAT.md`, `docs/cable-system/*`.
- Système d'éléments structuraux couvrant : `Beam`, `Column`, `TrussMember`, `Cable`
  (linéaires), `Slab`, `Wall` (surfaciques), `Foundation` (position dans la hiérarchie à
  vérifier).
- Système de sections centralisé (`Rectangular`, `Circular`, `IShape`, `Pipe`,
  `BoxHollow`, `UPN`, `Angle`, `TSection`).
- Système de matériaux avec 15 types prédéfinis et séparation propriétés
  mécaniques/visuelles.
- Système d'extensions dynamique TSALib (chargement à chaud, sans recompilation).
- Système de coordonnées avec grilles X/Y/Z paramétriques et gestion de niveaux d'étage.

## Points d'Architecture Vérifiés dans le Code Source (`VERIFIED IN SOURCE`)

Tous les points d'investigation initiale ont été rigoureusement analysés et vérifiés dans le code source :

- **Position exacte de `Foundation`** : `Foundation` (`src/Model/Foundation.h`) est une entité autonome (elle ne dérive ni de `LinearElement` ni de `SurfaceElement`). Elle est associée à un nœud d'appui (`nodeId`), possède ses propres dimensions géométriques de semelle (`widthA`, `lengthB`, `heightH`), une contrainte admissible de sol `soilBearingCapacity` (en kPa), et est gérée directement par `Model` (`addFoundation`, `getFoundation`, `removeFoundation`).
- **Usage exact de `ModelDiff`** : `ModelDiff` (`src/Model/ModelDiff.h`) calcule le différentiel structurel (éléments créés, modifiés, supprimés pour nœuds, poutres, poteaux, dalles, voiles, fondations, treillis, câbles) entre deux instantanés de modèle (`ModelStateSnapshot`). Il permet des rafraîchissements incrémentaux O(delta) du viewport OCCT (`OccView`), de l'arbre (`ModelTreeWidget`) et de l'Undo/Redo sans reconstruction globale de la scène.
- **Rôle de `InteractionManager`** : `src/Interaction/InteractionManager` centralise les états d'interaction 3D du viewport (`InteractionMode` pour la sélection, le dessin interactif de chaque type d'élément et les transformations 3D). Il gère également les requêtes de capture asynchrones (`SelectionRequest`) utilisées par les dialogues modaux pour sélectionner des coordonnées ou nœuds dans la vue 3D.
- **Rôle de `src/Diagnostics`** : Fournit le système central de diagnostic et de robustesse : capture de crash Windows avec génération de mini-dumps `.dmp` (`CrashHandler`), journalisation thread-safe en tampon circulaire avec 6 niveaux (`Logger`, `RingBuffer`), et génération de rapports de diagnostic système complets (`DiagnosticReport`).
- **Bootstrap de l'application** : `src/main.cpp` instancie `Application` (`src/App/Application.cpp`). Le cycle d'initialisation installe les handlers de crash et de log, configure le thème Fusion AutoCAD Dark, enregistre l'AppUserModelID Windows, configure les variables d'environnement OCCT (shaders, units) et l'association de fichiers `.tsa`, puis instancie et affiche `MainWindow`.
- **API de `MaterialLibrary`** : `src/Model/MaterialLibrary` est un singleton fournissant les matériaux génie civil normés (Eurocodes), la gestion des matériaux personnalisés, et la synchronisation bidirectionnelle avec les bibliothèques dynamiques chargées à chaud via **TSALib**.
- **Propagation `IModelObserver` vers l'UI** : `IModelObserver` (`src/Model/Model.h`) est implémenté directement par `OccView`, `ModelTreeWidget` et `PropertyPanel`. Le modèle structural reste la source unique de vérité et notifie ses observateurs sans dépendance circulaire. Les composants UI synchronisent leurs widgets ou émettent des signaux Qt secondaires si nécessaire.

## Ce document n'est pas

- Une roadmap produit/fonctionnelle (dates, priorités business) — ce fichier documente
  l'état technique connu et certifié pour l'usage des agents IA.
- Un remplacement de `docs/TSA_DIAGNOSTICS.md`, `docs/TSALIB_SYSTEM.md` ou
  `docs/TSA_FILE_FORMAT.md`, qui restent les références sur leurs sujets respectifs.

## Mise à jour

Ce fichier est maintenu synchronisé au fil des évolutions architecturales certifiées du dépôt.
