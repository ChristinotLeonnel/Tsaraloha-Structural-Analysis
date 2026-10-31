# Audit de qualité du code et de maintenabilité

## Conclusion

La qualité de surface du C++ est **bonne**. L'analyse statique sur 166 fichiers critiques ne révèle aucun usage après déplacement, aucun pointeur pendant et aucun vrai déréférencement nul. La duplication est faible et il ne reste aucun `TODO`.

Les faiblesses sont structurelles : quelques fonctions et classes très volumineuses, des frontières d'exception absentes et une documentation interne en retard sur le code.

## Analyse statique

`clang-tidy` de Visual Studio, familles `bugprone-*`, `clang-analyzer-*`, `performance-*` (sélection), `concurrency-mt-unsafe`, `cert-err58-cpp`. Modules : Model, UndoRedo, IO, Analysis, Standards, Mesh, Automation, Plugins, Commands, ExtensionSystem, BIM, Blueprint, Viewer, Project, Diagnostics, AI/Core, AI/Tools.

Bilan : **166 fichiers, 110 avertissements uniques dans `src/`**. Détail dans `preuves/resultats/tidy-summary.txt`.

| Règle | Nombre | Verdict après revue |
|---|---:|---|
| `bugprone-exception-escape` | 57 | Pour l'essentiel des constructeurs de copie en `noexcept` implicite sur manque de mémoire. **Utile sur 2 sites** : `AnalysisController.cpp:110` et `AIOrchestrator.cpp:166`, qui confirment BUG-155 |
| `cert-err58-cpp` | 26 | `QString` ou `std::string` statiques dans `MeshDisplayModel`, `MeshProject`, `MeshVisual`, `TSAFile.cpp:263`. Sans gravité, à rendre `constexpr` |
| `bugprone-empty-catch` | 12 | À relire : erreurs avalées sans trace |
| `clang-analyzer-core.NonNullParamChecker` | 4 | **4 faux positifs** vérifiés à la main : gardes à `AIToolRegistry.cpp:257` et `BimModel.cpp:587` |
| Compteur de boucle flottant | 6 | `OccView_Shapes.cpp:2824`, `WorkPlaneGrid.cpp:24` : accumulation d'arrondis possible, affichage seulement |
| `misplaced-widening-cast` | 1 | `WorkPlaneGrid.cpp:73`, conversion après le calcul, sans effet aux tailles usuelles |
| Autres | 4 | `atoi` sans contrôle (`OpenSeesMeshAdapter.cpp:24`), affectation dans un `if`, copie inutile |

**Limite** : 12 fichiers ont subi une erreur d'analyse de clang (macros Qt, ou `M_PI` sans `_USE_MATH_DEFINES`), donc une couverture partielle : `AIOrchestrator`, `OpenSeesManager`, `IfcStepWriter`, `TSAPreviewGenerator`, `Tsa3dExport`, `GmshMeshEngine`, `ProjectManager`, `ProjectTemplates`, `RecentProjects`, `SteelDesignEC3`, `OccView_Events`, `TextureManager`. Le dossier `src/UI` n'a pas été analysé (60 468 lignes).

## Pratiques C++ et Qt observées

| Sujet | Mesure | Appréciation |
|---|---|---|
| Mémoire | 19 `delete` explicites, en majorité des widgets Qt ou des threads | Sain : RAII et propriété Qt dominants |
| `const_cast` | 12 | À reprendre : `LibraryManager.cpp:272-294` (chargement paresseux dans une méthode `const`), `ProjectManager.cpp:143` |
| `catch (...)` | 65 | Beaucoup, à examiner : seuls les `catch` sans trace sont gênants |
| Singletons | 36 accesseurs `instance()` | Testabilité réduite, voir ARC-06 |
| Threads | 9 `QThread::create` | **Un seul corps de thread protégé par `try`** : `MainWindow_Bim.cpp` (BUG-155) |
| Signaux et lambdas | 3 `connect` sans objet de contexte | Sûrs : deux sur un `QProcess` local, un sur un membre du modèle |
| Accès concurrents | Calcul sur copie, `std::atomic` et mutex sur l'annulation | Sain à la lecture. Aucun outil de détection (TSan) disponible sous MSVC |
| OCCT | `Handle` partout, vue libérée sur destruction de surface (BUG-119) | Sain à la lecture |
| Exceptions et Qt | Pas de surcharge de `QApplication::notify` | Une exception dans un gestionnaire arrête l'application (BUG-155) |
| Comportement indéfini à la lecture | Enums lus du fichier sans contrôle, `static_cast` direct | Le fuzzing n'a produit aucun plantage par ce biais sur 3 000 fichiers en export |

Aucun test mémoire n'a été exécuté : **ASan, Valgrind et Dr. Memory n'ont pas été lancés**. L'absence de fuite ou d'écriture hors limites n'est donc pas démontrée sous Windows.

## Sécurité

| Surface | Constat | Appréciation |
|---|---|---|
| Lecture `.tsa` | Accès bornés, tailles contrôlées, 3 000 fichiers fabriqués sans plantage en export | Bon. Manque le contrôle référentiel (BUG-154) |
| Extension Shell (dans `explorer.exe`) | Lecture bornée à 8 Mio, `catch (...)`, aucune dépendance | Bon, relue intégralement |
| Serveur d'automatisation | `QLocalServer` en `UserAccessOption`, compte de l'utilisateur seulement | Bon |
| Script Tcl | Textes libres assainis par `tclComment` (BUG-108) | Bon |
| Paquets `.tsalib` | Contrôles BUG-109 et BUG-118 en place, relecture hors périmètre | Non refait |
| Greffons | DLL chargées sans signature depuis le dossier de l'application | Risque R-05 |
| Clés d'API | DPAPI sous Windows, en clair ailleurs (BUG-168) | Moyen |
| Chiffrement `.tsa` | XOR, non exposé (BUG-167) | À retirer |

## Complexité

Mesure approchée (décompte des décisions) sur 6 324 fonctions : `preuves/resultats/quality.txt`.

| Indicateur | Valeur |
|---|---:|
| Fonctions à complexité > 25 | 181 |
| Fonctions à complexité > 50 | 45 |
| Corps de plus de 200 lignes | 49 |
| Corps de plus de 400 lignes | 11 |

| Fonction | Complexité | Lignes | Emplacement |
|---|---:|---:|---|
| `MainWindow::createDockWindows` | 217 | 493 | `UI/MainWindow_Actions.cpp:1726` |
| `OpenSeesMeshAdapter::check` | 196 | 679 | `Mesh/Solvers/OpenSeesMeshAdapter.cpp:100` |
| `NDCGenerator::generate` | 144 | 852 | `NDC/NDCGenerator.cpp:104` |
| `StructuralClipboard::pasteTo` | 135 | 484 | `Model/StructuralClipboard.cpp:490` |
| `analyzeMesh` | 134 | 382 | `Mesh/MeshQuality.cpp:54` |
| `TSAFileReader::parsePayload` | 111 | 366 | `IO/TSAFile.cpp:914` |

Les fonctions les plus risquées sont celles qui portent une logique métier : `OpenSeesMeshAdapter::check`, `NDCGenerator::generate`, `pasteTo`. `createActions` et `ThemeManager` sont longues mais presque sans branches.

## Duplication et code mort

- **Duplication** : 171 blocs de 12 lignes répétés, soit 982 positions sur 180 000 lignes. Faible. Principaux couples : `BeamPropertiesView` / `ColumnPropertiesView` (29), `SurfaceLoadsDialog` / `SurfaceSupportsDialog` (11).
- **Duplication de logique** : plus grave que celle du texte, voir ARC-03.
- **Code mort** : 763 méthodes n'apparaissent que dans leur déclaration et leur définition. **Ce sont des candidats, pas des preuves** : signaux Qt, API de greffons, usage par TSALab (non inclus dans l'analyse). Rien ne doit être retiré sans vérification, comme le veut AGENTS.md.
- **`TODO`, `FIXME`, `HACK`** : 0 dans `src/`.
- **Taille** : 5 fichiers de plus de 2 000 lignes, 28 de plus de 1 000, 0 de plus de 4 000.

## Documentation

| Document | Écart avec le code |
|---|---|
| `.claude/current-state.md:3` | « Last Updated: 2026-10-07 » alors que des sections datent du 31/10 |
| `.claude/current-state.md:228` | « Meshing : MISSING » alors que `src/Mesh` et Gmsh existent |
| `.claude/current-state.md:235` | IFC « MISSING » alors que `src/BIM/IFC` existe |
| `.claude/current-state.md:231` | Format `.tsa` « 1.4 » au lieu de 2.0 |
| `.claude/current-state.md:204-209`, `.claude/CLAUDE.md:75` | « Statique seul » alors que le code a l'analyse modale |
| `.claude/architecture.md:126` | « Custom2D : solveur non connecté » alors qu'il calcule |
| `.claude/architecture.md:136` | « Exécution synchrone sur le thread UI » alors qu'un thread existe |
| `.claude/architecture.md:116` | Format « 1.1 » |
| `docs/AUDIT_CALCULS.md:44` | Relâchements marqués « T » (testés) : c'est exact, mais sans la combinaison qui échoue (BUG-148) |
| `tests/test_main.cpp:11` | `expectedTotal = 530` alors que 538 tests passent |
| `docs/shortcut.txt` et `docs/shortcuts.txt` | Deux fichiers au nom proche |

## Conventions

Nommage homogène (`m_`, `camelCase`, `PascalCase`), commentaires en français, inclusions à casse correcte (0 écart), aucun antislash dans une inclusion. Organisation des fichiers lisible, avec `MainWindow_*.cpp` et `OccView_*.cpp` comme découpage d'une même classe.
