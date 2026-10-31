# Audit multiplateforme

## Conclusion

**Windows MSVC Release est validé sur ce poste.** Linux n'a pas pu être vérifié : aucune distribution WSL ni compilateur GCC n'est disponible ici. Windows Debug non plus : aucun binaire Debug n'existe. Tout ce qui concerne Linux ci-dessous vient de la lecture du code et de la documentation existante.

## Ce qui a été réellement vérifié

| Environnement | État | Preuve |
|---|---|---|
| Windows 11, MSVC 19.51, Qt 6.12.0, Release | **VALIDÉ** : 57/57 suites, 538 tests | `preuves/resultats/tests-report.md` |
| OCCT 8.0.1 (vc14) | Validé par la suite ci-dessus | |
| OpenSees 3.8.0 (conda-forge) | Validé, y compris copie de `thirdparty/OpenSees` | Essai masqué, 851 fichiers identiques |
| Gmsh 4.15.2, IfcOpenShell 0.9.0post1 | Validés par les suites `mesh`, `surfaces`, `ifc` | |
| Windows Debug | **NON VÉRIFIÉ** | `build-ninja-debug` sans exécutable |
| Windows MinGW | **NON VÉRIFIÉ** | Évoqué par AGENTS.md, aucun outil présent |
| Linux GCC, Debug et Release | **NON VÉRIFIÉ ici** | `build-linux-*` existent (binaires ELF du 26/10), non exécutables sur ce poste |
| Clang | **NON VÉRIFIÉ** | `clang-tidy` seul, 12 fichiers mal analysés |
| CI GitHub | **NON CONSULTÉE** | `gh` absent |

Dernier état Linux documenté, non revérifié : Release 503/504 le 2026-10-26 (`.claude/current-state.md`). Il est antérieur à l'API d'édition, à l'espace de travail Blueprint et aux courbes 2D.

## Portabilité du code

| Sujet | Constat |
|---|---|
| Code conditionnel | 34 fichiers. Cloisonné pour l'essentiel : `Platform/WindowsAssociation`, `CrashHandler`, `HardwareProfiler`, `SecretStore`, `ProcessLifetime` |
| Inclusions | **0 erreur de casse, 0 antislash** sur tout `src/` : bon pour Linux |
| Chemins absolus dans `src/` | Aucun qui soit bloquant. Repli `C:/Windows` à `OpenSeesManager.cpp:317`, exemples en commentaire à `ExtensionPackager.h:61` |
| Chemins en dur dans les outils | `tools/build_installer.ps1:37` (profil personnel), `tools/i18n/update_translations.py:34` |
| Chemins UTF-8 | Corrigés par `TSA::Core::utf8Path` (BUG-131, BUG-136). Reste `TSALab/lab/Tests/LabTests.cpp:120` (W-6) |
| `M_PI` | 40 usages sans `_USE_MATH_DEFINES` : dépend d'un en-tête de Qt ou d'OCCT, fragile avec Clang |
| Extension Shell, tube nommé, DPAPI | Fonctions propres à Windows, sans équivalent Linux pour les miniatures. Clé d'API en clair hors Windows (BUG-168) |
| `OpenSeesPlanarSolver` | Deux chemins de code : `CreateProcessW` masqué sous Windows, `std::system` ailleurs |

## Compilation et dépendances

| Sujet | Constat | Gravité |
|---|---|---|
| Presets | `ninja-*` et `linux-*` cohérents. Qt détectée automatiquement | — |
| Dépendance à des dépôts voisins | `../TSALab` obligatoire, `../3DDesplacement` facultatif, sans version épinglée | MEDIUM |
| Dossier vide masquant OpenSees | `AppPaths::openSeesDir` retient le premier dossier existant, même sans exécutable (noté dans BUG-146). Différence de comportement : TSA lit `engines/OpenSees`, TSALab et Fem3D lisent `thirdparty/OpenSees` | MEDIUM |
| `FEM3D_DEFAULT_OPENSEES` | Chemin figé à la compilation pour le banc Fem3D | LOW |
| Optimisation Release | `/O2 /Ob2` sans optimisation globale | INFORMATIONAL |
| Dépôts Git voisins | Propriétaire Windows différent : `dubious ownership` sur `TSALab` et `3DDesplacement` | INFORMATIONAL |
| Worktrees | 4 entrées `prunable` vers `/media/tsaraloha-dev/...` | LOW, informatif : ne rien lancer sans accord |

## Intégration continue

Lu dans `.github/workflows/ci.yml`, non exécuté.

| Point | Constat | Gravité |
|---|---|---|
| OpenSees absent de la CI | Le **moteur par défaut n'est jamais validé** : suites `NON EXÉCUTÉE` | HIGH |
| Windows CI | Qt 6.10.3 et MSVC 19.44, différents du poste | MEDIUM |
| Actions | Non épinglées par SHA (noté dans le fichier) | LOW |
| Bloquant | Non, le fichier précise qu'il reste à rendre bloquant | MEDIUM |

## Licences

Hors périmètre technique, mais bloquant pour une diffusion : la racine est sous licence MIT, alors que Gmsh est sous GPL v2+ et qu'OpenSees a une licence propre de l'Université de Californie. `docs/THIRD_PARTY_LICENSES.md` qualifie déjà Gmsh de « point bloquant pour une diffusion non libre ». Une décision reste ouverte. Cet audit n'est pas un avis juridique.
