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

## `TODO: VERIFY IN SOURCE`

Points identifiés lors de la rédaction de ce framework, à vérifier/compléter au fil de
l'eau plutôt que supposés :

- Position exacte de `Foundation` dans la hiérarchie `Element`.
- Usage exact de `ModelDiff`.
- Détail de `src/Interaction/InteractionManager` (évènements gérés).
- Détail de `src/Diagnostics` (nature de la télémétrie).
- Détail de `src/Project`, `src/App`, `src/main.cpp` (bootstrap exact de l'application).
- API complète de `MaterialLibrary`.
- Chaîne exacte de propagation entre `IModelObserver` et les signals Qt de l'UI.

## Ce document n'est pas

- Une roadmap produit/fonctionnelle (dates, priorités business) — ce fichier documente
  l'état technique connu et les zones à vérifier, pour l'usage des agents IA.
- Un remplacement de `docs/TSA_DIAGNOSTICS.md`, `docs/TSALIB_SYSTEM.md` ou
  `docs/TSA_FILE_FORMAT.md`, qui restent les références sur leurs sujets respectifs.

## Mise à jour

Ce fichier doit être mis à jour à chaque fois qu'un point `TODO: VERIFY IN SOURCE` de ce
framework (ici ou dans les autres documents `docs/`) est vérifié et clarifié dans le code.
