# Rapport de validation Windows — TSA

| | |
|---|---|
| Date | 2026-10-28 (18:05 → 19:10, heure locale) |
| Commit testé | `6a5bde1` (branche `test/etape2-caracterisation` = chantier C `3b65ac3` + un commit de documentation) |
| Branche de travail | `test/windows-validation` (créée depuis `6a5bde1`) ; corrections **non commitées** |
| Configuration | Release, Ninja, MSVC x64, dossier séparé `build-windows-validation` (dossiers existants non touchés) |
| Recommandation | **PARTIEL** : compilation et tests exécutables VALIDÉS ; calcul OpenSees BLOQUÉ (exécutable absent, serveur officiel en panne) |

Statuts : **VALIDÉ** (exécution réussie prouvée) · **ÉCHEC** · **BLOQUÉ** (dépendance ou condition externe) · **NON TESTÉ** · **PARTIEL**.

## 1. Environnement détecté

| Élément | Version / état | Remarque |
|---|---|---|
| Windows | 10 Pro 22H2 (10.0.19045), x64, 12 processeurs logiques, 15,8 Go | |
| Visual Studio | 18 Community (catalogue 18.11.2) | `vswhere` absent du PATH : message inoffensif de `vcvars64.bat` |
| MSVC | `cl` 19.51.36260, outils 14.51.36231, Hostx64/x64 | `cl`, `lib`, `dumpbin`, `link` dans le même dossier |
| Windows SDK / UCRT | 10.0.26100.0 | |
| CMake | 4.5.0-rc1 (`C:\Program Files\CMake`) | version préliminaire (rc) |
| Ninja | 1.13.2 (fourni par Visual Studio) | |
| Git | 2.56.0.windows.4 | dépôt détenu par un autre SID (disque issu d'un autre poste) : `git -c safe.directory=…` utilisé, configuration globale non modifiée |
| Python | 3.15.2 (`C:\Python315`, lanceur `py`) | installation incomplète (pas de `Lib\`), bibliothèque standard retrouvée ailleurs : fonctionne, avec un avertissement à chaque lancement ; `python` du PATH = alias Microsoft Store (bouchon, code 9009) |
| **Qt** | **6.12.0** `msvc2022_64` | **Qt 6.11.2 absent** alors que `CMakePresets.json` l'impose : presets non utilisables tels quels, Qt passé en option (`-DQt6_DIR`) |
| OpenCASCADE | 8.0.1 `opencascade-8.0.1-vc14-64` + `3rdparty-vc14-64` (VTK 9.4.1 inclus, non utilisé par TSA) | |
| Gmsh | SDK 4.15.2 (`thirdparty/gmsh-4.15.2-Windows64-sdk`) | |
| IfcOpenShell | 0.9.0post1 (13 bibliothèques) | |
| **OpenSees** | **absent** | voir §5, défaut W-4 |
| TSALab | `../TSALab` @ `eea9cd9` (branche `build/tests-qtwidgets-explicite`, chantier B1) | |
| Chemins | dépôt avec espace (`D:\Tsaraloha Platforme`) ; profil utilisateur non ASCII (`C:\Users\T. Léonnel`, TEMP compris) | condition révélatrice des défauts W-1 et W-6 |

## 2. Commandes exécutées (principales)

```text
cmake -S . -B build-windows-validation -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl
      -DQt6_DIR=C:/Qt/6.12.0/msvc2022_64/lib/cmake/Qt6 -DOpenCASCADE_DIR=<TSA>/opencascade-8.0.1-vc14-64/cmake
      -DTSA_AUTO_DOWNLOAD_DEPS=OFF -DTSA_FEM3D_DIR=<P>/3DDesplacement -DTSALAB_ROOT_DIR=<P>/TSALab   (sous vcvars64, VSLANG=1033)
cmake --build build-windows-validation -- -k 0 -j 4
python tools/ci/run_test_suites.py --exe build-windows-validation/TSA_TestSuite.exe --out … --available … --timeout 900
ctest --test-dir build-windows-validation -R "Layers|I18nCheck|CoreLink|CoreStaticInit" --output-on-failure
build-windows-validation/TSA_Benchmark.exe --engine custom2d --out …
python tools/ci/check_dependencies.py ; tools/check_layers.py ; tools/check_shortcuts.py --strict ; tools/i18n/check_translations.py --strict
TSALab : même configuration (-DTSA_ROOT_DIR=<P>/TSA), cmake --build, TSALab_TestSuite.exe
TSA.exe (lancement, fermeture par WM_CLOSE) ; tsa-mesh.exe et tsa-ifc.exe export sur examples/TSA_Exemple_Dalle_Coques.tsa
```

Journaux : `build-windows-validation/wv-report/` (non versionné) — `tests/` (avant corrections), `tests-after/` (après),
`ascii/` (contre-épreuve TEMP ASCII), `after/`, `bench/`, `tsalab/`, `cli/`.

## 3. Configuration et compilation

| Vérification | Statut | Preuve |
|---|---|---|
| Configuration CMake TSA | VALIDÉ | code 0 ; Gmsh et IfcOpenShell détectés ; page de code MSVC 65001 (`msvc_codepage.cmd`) |
| Compilation complète TSA (553 cibles) | VALIDÉ | code 0, 7 min 52 s, -j 4, aucune erreur de compilation ni d'édition de liens |
| Exécutables produits | VALIDÉ | `TSA`, `TSA_TestSuite`, `TSA_CoreTests`, `TSA_Benchmark`, `tsa-mesh`, `tsa-ifc`, `tsa-template`, `tsa3d-validate`, `tsaraloha-mcp`, `TSAThumbnailProvider.dll` |
| Déploiement des DLL (OCCT, 3rdparty, Qt par windeployqt) | VALIDÉ | DLL présentes ; avertissement « DLL introuvables : Qt6… » trompeur (W-8) |
| Configuration et compilation TSALab | VALIDÉ | code 0, 6 min 55 s ; recompilé après la modification de `src/` |
| Avertissements | 27 (TSA) | C4996 ×12 (dépréciations OCCT 8, comme sous Linux), C4456 ×9 et C4458 ×2 (masquages), C4834 ×3 (`[[nodiscard]]` ignoré, tests seulement), C4100 ×1. Aucun dans un chemin de calcul. La mémoire du projet indiquait « aucun connu » : à mettre à jour |
| Contrôle CoreStaticInit sous MSVC | NON TESTÉ | non enregistré sur ce commit (`TSAProduct.cmake:1798`, `NOT MSVC`) ; le moteur dumpbin (A2) n'est pas intégré |

## 4. Tests

### 4.1 Exécution complète avant corrections (toutes dépendances déclarées présentes)

57 suites exécutées : **38 réussies, 19 en échec** ; 405 tests réussis, 19 échecs, 1 ignoré (`ailive`, modèle IA en direct).

| Cause des 19 échecs | Suites | Classement |
|---|---|---|
| OpenSees introuvable | opensees, ndc, extraction, engines, mdd, displaymodes, tsa3d, templates, torsion, demo, surfaces, audit, benchmarks, display (14) | BLOQUÉ |
| Chemin UTF-8 étroit dans le test (TEMP non ASCII) | thumbnail (127), mesh (293), meshview (341) | défaut W-1 |
| Alias Python du Microsoft Store choisi | ai (489), operator (489) | défaut W-2 |

### 4.2 Exécution complète après corrections

57 suites : **42 réussies, 14 non exécutées (OpenSees absent), 1 en échec** ; 390 tests réussis, 1 échec, 1 ignoré.
Le seul échec, `mesh` au test 294 (calcul OpenSees), venait de la table des dépendances de l'outil (W-5), corrigée
après cette exécution : `mesh` est désormais classée NON EXÉCUTÉE sans OpenSees.

### 4.3 Détail par domaine

| Domaine (§5 de la mission) | Suites | Statut |
|---|---|---|
| Modèle structurel, création / modification / suppression | model 21, node 3, supports 6, cables 3, loads 14, topology 12, cleanup 8, groupedit 3 | VALIDÉ |
| Undo / Redo | undo 11/11 (chantier C, tests 513-523), commands 7 | VALIDÉ (identique à Linux) |
| Sauvegarde / chargement / sérialisation | io 9, thumbnail 3 (dont **test 128 Explorateur, Windows seul**), preview 3, templates_wp 8 | VALIDÉ |
| Matrices, assemblage, conditions aux limites, résultats | banc Custom2D **58/58** ; suites extraction, engines, benchmarks | VALIDÉ (Custom2D) · BLOQUÉ (OpenSees) |
| Interface Qt, rendu OCCT | views **19/19** (pilote graphique réel du poste), viewer 10, display… (OpenSees), multidoc 10, ribbon 4, window 1, snap 8, dimensions 13, appearance 6, i18n 15 | VALIDÉ (sauf `display` BLOQUÉ) |
| IFC / Gmsh | ifc 10, bim 15, drawing 25, mesh 5/6, meshview 12 | VALIDÉ (calcul des maillages par OpenSees BLOQUÉ) |
| Scripts et outils | 4 contrôles Python (dépendances, couches, raccourcis, traductions), CTest CoreLink / Layers / I18nCheck 3/3, `tsa-mesh`, `tsa-ifc export` | VALIDÉ |
| IA, automatisation, extensions | ai 24, operator 8, automation 3, extensions 12, blueprint 9, modules 6 | VALIDÉ |
| TSALab | `TSALab_TestSuite` : 17/27 avec TEMP non ASCII (W-6) ; **27/27 avec TEMP ASCII**, 6 parties ignorées (toutes OpenSees, L9-L14) | PARTIEL |
| Lancement de l'application | `TSA.exe` : fenêtre « TSA — Start Center » en 0,6 s, répond, Qt 6.12 / OCCT / IfcOpenShell chargés, fermeture propre code 0 | VALIDÉ |

### 4.4 Validation numérique

`TSA_Benchmark --engine custom2d` : **58 vérifications réussies, 0 échec** sur 21 cas RDM, comparées aux formules
analytiques (par exemple δ = PL/EA, réactions, contraintes) avec la tolérance mixte |x − x_réf| ≤ 1e-9 + 1e-6·|x_réf| ;
écarts relatifs observés de l'ordre de 1e-14. Même résultat que la CI Windows (58/58) et Linux.
Les cas FEA et les cas calculés par OpenSees (28 cas ignorés par la suite `benchmarks`) : **BLOQUÉ**.

## 5. Défauts découverts

| Id | Gravité | Domaine | Description | Statut |
|---|---|---|---|---|
| W-1 | MOYENNE | tests TSA | `test_thumbnail_provider.cpp:66`, `test_mesh.cpp:329/337`, `test_meshview.cpp:459` ouvrent ou testent un chemin UTF-8 via une chaîne étroite (`std::fstream`, `std::ofstream`, `std::filesystem::exists`), que MSVC interprète dans la page de code ANSI : échec dès que TEMP contient « é ». Contraire à la règle de `src/Core/Utf8Path.h`. Le code de production (E/S `.tsa`, Gmsh) gère ces chemins ; vérifié pour Gmsh par un essai direct de la DLL (écriture et relecture dans un dossier `…_é`) | **CORRIGÉ** (`TSA::Core::utf8Path`), VALIDÉ |
| W-2 | MOYENNE | production, IA | `PythonAIBridge::findPython()` prenait le premier `python` du PATH, l'alias `WindowsApps\python.exe` présent par défaut sur Windows 10/11 (0 octet, code 9009) : couche IA Python inutilisable, test 489 en échec | **CORRIGÉ** (sous Windows, `py` d'abord), VALIDÉ |
| W-3 | ÉLEVÉE (OpenSees) | production, serveur externe | `OpenSeesManager::officialDownloadUrl()` (`https://opensees.berkeley.edu/OpenSees/code/OpenSees3.8.0-x64.exe.zip`) donne une 404 : le site redirige toutes ses pages vers `https://peer.berkeley.edu/openseesOpenSees/…` (barre oblique manquante côté serveur). Le téléchargement intégré à l'application échoue. Il ne vérifie pas non plus l'empreinte de l'archive | OUVERT (dépend du serveur ; vérification d'empreinte à ajouter) |
| W-4 | ÉLEVÉE (environnement) | dépendances | `thirdparty/OpenSees/bin/gOpenSees.exe` a été mis à la Corbeille à 18:08, pendant la compilation, par une action extérieure à la session (aucune étape CMake ni aucun test n'y touche ; la seule mise en Corbeille du code, `StartCenter.cpp:602`, n'a pas été exécutée). Sur demande de l'utilisateur : téléchargement automatique ajouté | **AJOUTÉ** (§6) ; mécanisme VALIDÉ, téléchargement réel BLOQUÉ (W-3) |
| W-5 | FAIBLE | outillage CI | `tools/ci/run_test_suites.py` : `mesh` déclarée dépendante de Gmsh seulement, alors que le test 294 calcule avec OpenSees | **CORRIGÉ** |
| W-6 | MOYENNE | tests TSALab | `TSALab/lab/Tests/LabTests.cpp:120` : même défaut que W-1 (`std::fstream f(path.toStdString(), …)`) ; L5 échoue et L6-L14 ne s'exécutent pas | OUVERT : correctif d'une ligne (`TSA::Core::utf8Path`) non appliqué, car TSALab est un autre dépôt dont la branche est en attente de validation |
| W-7 | FAIBLE | tests TSA | `test_torsion.cpp:724` : `std::filesystem::remove` sur un chemin UTF-8 étroit ; laisse un fichier dans TEMP (suite non atteinte ici) | **CORRIGÉ**, compilé, NON EXÉCUTÉ (la suite s'arrête au test 321 sans OpenSees) |
| W-8 | FAIBLE | build | `DeployDependencies.cmake:30` : la liste « DLL introuvables » n'applique pas `EXCLUDE_REGEXES "^[Qq]t6"` ; Qt est bien déployé par windeployqt | **CORRIGÉ** (vérifié : plus d'avertissement) |
| W-9 | FAIBLE | environnement / presets | Qt 6.11.2 imposé par `CMakePresets.json` mais absent ; poste en Qt 6.12.0 | **CORRIGÉ** (§11) : plus aucun chemin Qt dans les presets, détection automatique ; cache périmé remplacé (essai m3) |
| W-10 | MOYENNE | production (diagnostic) | `Logger::init` : chemins UTF-8 étroits (`std::ofstream`, `std::filesystem`) ; dans le paquet installé (journaux dans `AppData`, profil « T. Léonnel »), **aucun journal n'était écrit** (« Impossible d'ouvrir le fichier de log ») ; invisible depuis le dossier de compilation (chemin ASCII) | **CORRIGÉ** (`TSA::Core::utf8Path`), test 37.8 (échoue sans le correctif, réussit avec) ; journal constaté dans `AppData\Local\TSA Engineering\TSA\logs` après relance du paquet |

## 6. Corrections effectuées (non commitées, branche `test/windows-validation`)

| Fichier | Modification |
|---|---|
| `tests/test_thumbnail_provider.cpp` | `downgradeToV11` : `TSA::Core::utf8Path(path.toStdString())` (W-1) |
| `tests/test_mesh.cpp` | `std::filesystem::exists(TSA::Core::utf8Path(…))` aux lignes 330 et 338 (W-1) |
| `tests/test_meshview.cpp` | `std::ofstream f(TSA::Core::utf8Path(path))` (W-1) |
| `src/AI/Python/PythonAIBridge.cpp` | sous `Q_OS_WIN`, ordre `py`, `python`, `python3` ; Linux inchangé (W-2). Un filtre « alias de 0 octet » a été écarté : un Python installé depuis le Store est lui aussi un alias de 0 octet |
| `cmake/SetupDependencies.cmake` | `tsa_download_opensees()` : si `thirdparty/OpenSees` ne contient aucun exécutable, téléchargement de `TSA_OPENSEES_URL`, contrôle SHA-256 (`TSA_OPENSEES_SHA256` = `5E37A5A9…FB41`, manifeste winget), installation de `bin/` et `lib/` ; avertissement seulement en cas d'échec ; Windows seulement, sous `TSA_AUTO_DOWNLOAD_DEPS` (W-4) |
| `tools/ci/run_test_suites.py` | `"mesh": ["gmsh", "opensees"]` (W-5) |
| `tests/test_torsion.cpp` | test 332 : `std::filesystem::remove(TSA::Core::utf8Path(path))` (W-7) |
| `cmake/RuntimeDependencies.cmake` | `tsa_copy_runtime_dependencies` : la liste `UNRESOLVED_VAR` est filtrée par `EXCLUDE_REGEXES` (W-8 ; concerne aussi `PackageTSA.cmake`) |
| `DOCUMENTATION.md` §10.4 | téléchargement automatique d'OpenSees documenté |

Essais de `tsa_download_opensees` (`cmake -P`, destination temporaire) : serveur officiel → avertissement, rien
installé ; archive d'empreinte différente → rejetée ; archive conforme (archive d'essai, empreinte fournie) →
`bin/` et `lib/` installés ; exécutable présent → aucun téléchargement. Configuration réelle avec
`TSA_AUTO_DOWNLOAD_DEPS=ON` : avertissement, configuration réussie (code 0).

## 7. Avant / après

| Mesure | Avant | Après |
|---|---|---|
| Suites (57) | 38 réussies, 19 échecs | 42 réussies, 14 non exécutées (OpenSees), 1 échec → 0 après W-5 |
| thumbnail / mesh (293) / meshview | 0/3 · échec 293 · 6/12 | 3/3 · 293 réussi · 12/12 (TEMP non ASCII, condition d'origine) |
| ai / operator | 23/24 · 7/8 | 24/24 · 8/8 |
| TSALab | 17/27 | 27/27 avec TEMP ASCII seulement (W-6 ouvert) |
| Non-régression | — | 4 contrôles Python, CTest 3/3, suites complètes relancées, TSA et TSALab recompilés |

## 8. Comparaison Windows / Linux

| Point | Linux (rapports du 2026-10-24 au 26) | Windows (ce poste) |
|---|---|---|
| Compilateur | GCC 13.3, Qt 6.12.0 | MSVC 19.51, Qt 6.12.0 |
| Suite complète (C) | Release 521/522, test 128 ignoré | non comparable en total (OpenSees absent) ; test 128 **réussi** |
| undo | 11/11 | 11/11 |
| views | 19/19 (llvmpipe, GPU Intel) | 19/19 (pilote du poste) |
| Custom2D | 58/58 | 58/58 |
| TSALab | 27/27 | 27/27 (TEMP ASCII) |
| Chemins non ASCII | non observés (profil ASCII) | W-1 et W-6 révélés |
| Avertissements | dépréciations OCCT 8 | dépréciations OCCT 8 + masquages, `[[nodiscard]]` |

Aucun test Linux n'a été exécuté pendant cette mission (aucun environnement Linux disponible). Impact Linux des
corrections, établi par lecture : `utf8Path` est déjà utilisé par le code de production compilé sous Linux ;
`PythonAIBridge` change sous `Q_OS_WIN` seulement ; `setup_external_dependencies()` s'arrête avant la section
OpenSees hors Windows. Seule exception : avec W-5, `mesh` devient NON EXÉCUTÉE dans la CI Linux, qui n'a pas OpenSees
(elle échouait auparavant au test 294).

## 9. Limites et étapes restantes

- **BLOQUÉ** : les 14 suites OpenSees, les 28 cas OpenSees du banc, les parties OpenSees de TSALab. À relancer dès
  qu'OpenSees sera en place (serveur officiel rétabli, ou exécutable restauré depuis la Corbeille par l'utilisateur).
- **NON TESTÉ** : procédure `PROCEDURE_WINDOWS_v2.md` des worktrees A1, A2 (moteur dumpbin), B, B sans IFC ;
  configuration Debug ; analyse statique (`/analyze`) et sanitizers (non exécutés) ; CI GitHub (aucun push).
- **Vérifications graphiques manuelles restantes** : création de nœuds et de barres, appuis, sections, sauvegarde et
  réouverture, Annuler / Rétablir dans l'interface, sélection 3D, calcul depuis l'interface. Ces opérations sont
  couvertes par les tests automatisés au niveau du modèle, mais pas par une manipulation réelle (aucun outil de
  pilotage du bureau dans cette session).
- Décisions à prendre : W-3 (source et empreinte du téléchargement à l'exécution), W-6 (correctif TSALab), W-9 (Qt).

## 10. Recommandation

**PARTIEL — TSA n'est pas entièrement validé sous Windows.** La compilation MSVC 19.51 Release de TSA et de TSALab,
les 42 suites exécutables, le banc numérique Custom2D, le démarrage de l'application et les outils en ligne de
commande sont VALIDÉS. Les défauts de portabilité trouvés (W-1, W-2, W-5) sont corrigés et vérifiés. Le calcul
OpenSees, au cœur de l'analyse structurelle, reste BLOQUÉ. Il est à revalider avant toute fusion, de même que W-6
et la procédure des worktrees A2 / B.

## 11. Automatisation portable des dépendances (2026-10-28, 19:20 → 20:15)

Branche `build/dependency-detection` (créée depuis `test/windows-validation`, modifications précédentes conservées,
**non commitée**). Référence technique : **`docs/DEPENDENCIES.md`**.

### 11.1 Architecture existante et limites (audit)
- Déjà en place et conservé : `cmake/SetupDependencies.cmake` (SDK OCCT + 3rdparty téléchargés sous Windows, OpenSees),
  `Gmsh.cmake` → `TSA::Gmsh`, `IfcOpenShell.cmake` (`find_package CONFIG`), `RuntimeDependencies.cmake` (dumpbin +
  `GET_RUNTIME_DEPENDENCIES`, copies comparées par SHA-256), `PackageTSA.cmake` + `tools/verify_package.py`, scripts
  Linux (`scripts/linux/common.sh` : version la plus récente de `~/Qt/6.*`, `~/opt/occt-8.*`), scripts PowerShell
  (`setup_build.ps1`, `detect_qt.ps1`, `detect_dependencies.ps1`).
- Limites constatées : Qt figé à `C:/Qt/6.11.2` dans 3 presets (W-9) ; plancher `Qt6 6.2` non justifié — les
  traductions exigent en réalité Qt ≥ 6.9 (`qt_add_lrelease(TS_FILES … QM_OUTPUT_DIRECTORY)`) ; aucune vérification
  de chaîne de compilation, d'architecture ou de mélange dans CMake (seulement dans les scripts PowerShell, contournés
  par les presets) ; `find_program(WINDEPLOYQT_EXECUTABLE)` en cache : après un changement de Qt, windeployqt d'une
  autre installation ; DLL OCCT déployées depuis `opencascade-8.0.1-vc14-64` figé, même avec un autre
  `OpenCASCADE_DIR` ; aucun rapport des versions retenues ; documentation annonçant MinGW et VTK alors que le SDK OCCT
  est MSVC seul et que VTK n'est pas utilisé ; W-8 (§5) appliqué dans la fonction commune affaiblissait le contrôle
  strict du paquet (une DLL Qt non résolue n'y aurait plus été une erreur) : déplacé dans le seul déploiement de
  développement.

### 11.2 Système retenu (aucun gestionnaire parallèle : `SetupDependencies.cmake` étendu)
- `tsa_find_qt()` avant `find_package(Qt6)` : respecte `Qt6_DIR` / `CMAKE_PREFIX_PATH` ; sinon version la plus
  récente ≥ 6.2 de la chaîne du compilateur parmi `$env{Qt6_DIR,QTDIR,TSA_QT_DIR}` et `<racine>/6.*/<kit>`
  (`TSA_QT_SEARCH_ROOTS`) ; remplace un `Qt6_DIR` périmé et efface les `Qt6*_DIR` d'une autre installation.
- `tsa_validate_dependencies()` (appelé par `TSAProduct.cmake`, donc aussi pour TSALab sans modifier TSALab) :
  x64, Qt MSVC / MinGW / ELF selon le compilateur, kit `msvcAAAA` ↔ `MSVC_VERSION`, mélange de modules Qt, OCCT
  ≥ 8.0 et `vc*` avec MSVC ; statut VALIDÉE / NON VALIDÉE.
- Rapport : bloc « Dépendances retenues », `<build>/tsa-dependencies.txt`, cible `tsa-dependency-report`
  (`cmake/DependencyReport.cmake`, lecture seule : chemins encore présents, OpenSees recherché comme à l'exécution).
- windeployqt pris dans `bin/` de la Qt retenue ; DLL OCCT depuis `OpenCASCADE_BINARY_DIR` (déploiement et paquet).
- Traductions désactivées avec avertissement si Qt < 6.9 (au lieu d'un échec ou de ressources fausses).
- OpenSees : PATH respecté, `-DTSA_OPENSEES_ARCHIVE=<zip>` (même empreinte), délai de téléchargement borné ;
  application : `curl --fail` + contrôle SHA-256 avant extraction (`OpenSeesManager::officialArchiveSha256`), message
  explicite.

### 11.3 Versions (justification détaillée : `docs/DEPENDENCIES.md` §2)
| Dépendance | Acceptées | Validées (compilées et testées) |
|---|---|---|
| Qt | ≥ 6.2, < 7 (traductions ≥ 6.9) | 6.12.0 (ce poste, Windows ; Linux 2026-10-24) ; 6.11.2 (historique du poste) |
| OpenCASCADE | ≥ 8.0 | 8.0.1 |
| Gmsh | SDK officiel | 4.15.2 |
| IfcOpenShell | construit contre l'OCCT de TSA | 0.9.0post1 |
| OpenSees | ≥ 3.4.0 (contrôle au lancement) | 3.8.0 (antérieurement) — **non disponible sur ce poste** |

### 11.4 W-3 : source d'OpenSees
Constat : l'adresse officielle et la page de téléchargement d'`opensees.berkeley.edu` redirigent vers une adresse
PEER mal formée (404) ; les publications GitHub `OpenSees/OpenSees` (v3.8.0 du 2026-02-18) n'ont aucun binaire ; le
manifeste winget 3.8.0 désigne la même adresse ; la Wayback Machine n'a aucune copie. **Aucune source officielle
vérifiable de l'exécutable Windows n'est joignable : aucun miroir tiers n'a été ajouté.** Voies proposées :
exécutable existant (dont celui mis à la Corbeille le 2026-10-28, W-4), archive officielle déjà obtenue
(`TSA_OPENSEES_ARCHIVE`, empreinte winget exigée), ou compilation depuis l'étiquette `v3.8.0` (non automatisée sous
Windows).

**Suite (décision de l'utilisateur : « télécharger un nouvel OpenSees.exe »)** : l'ancien miroir
`opensees.ist.berkeley.edu` est lui aussi redirigé ; **conda-forge** publie OpenSees 3.8.0 win-64, compilé depuis
l'étiquette officielle `v3.8.0` par une recette publique. Paquets téléchargés directement (aucun installateur
exécuté), SHA-256 comparés à ceux d'anaconda.org, fermeture des DLL établie par dumpbin (13 DLL, 12 paquets),
`thirdparty/OpenSees` assemblé (`bin/`, `lib/tcl8.6`, `lib/tcl8`, `SOURCE.txt`) ; l'ancien `lib/` est conservé sous
`lib.avant-conda-20261028`. Automatisé dans CMake (`TSA_OPENSEES_CONDA_PACKAGES`) : installation par CMake
**identique octet pour octet** (849 fichiers) à l'assemblage manuel ; empreinte falsifiée → paquet rejeté, rien
installé, configuration non bloquée. **NON VALIDÉ À L'EXÉCUTION** : le lancement d'OpenSees.exe (et donc les 14
suites OpenSees, le banc, TSALab L9-L14) a été refusé par le contrôle de sécurité de la session (code externe
téléchargé) ; à exécuter par l'utilisateur ou avec son autorisation.

### 11.5 Fichiers modifiés
`CMakeLists.txt`, `CMakePresets.json` (Qt6_DIR retiré ×3), `cmake/SetupDependencies.cmake`, `cmake/TSAProduct.cmake`,
`cmake/Gmsh.cmake`, `cmake/IfcOpenShell.cmake`, `cmake/RuntimeDependencies.cmake`, `cmake/DeployDependencies.cmake`,
`cmake/DependencyReport.cmake` (nouveau), `src/Analysis/OpenSeesManager.{h,cpp}`, `src/Diagnostics/Logger.cpp` (W-10),
`tests/test_opensees.cpp` (491 : archive rejetée, empreinte identique CMake / C++), `tests/test_extensions.cpp`
(37.8), `docs/DEPENDENCIES.md` (nouveau), `DOCUMENTATION.md` §10, `docs/LINUX_BUILD.md`, ce rapport.

### 11.6 Essais réellement exécutés (Windows, MSVC 19.51, CMake 4.5.0-rc1, Ninja)
| Essai | Résultat |
|---|---|
| m1 : dossier neuf (espace + « é » dans le chemin), aucun Qt6_DIR | Qt 6.12.0 msvc2022_64 retenu ; MinGW et 4 kits Android écartés et listés ; rapport complet — VALIDÉ |
| m2 : Qt sous une racine personnalisée avec espaces (`-DTSA_QT_SEARCH_ROOTS`, jonction) | retenu depuis cette racine — VALIDÉ |
| m3 : cache périmé `Qt6_DIR` + `Qt6Core_DIR` vers 6.11.2 (ancien preset) | remplacé par 6.12.0, entrée périmée effacée, aucun mélange — VALIDÉ |
| m4 : `Qt6_DIR` = kit MinGW avec MSVC | arrêt, message ABI et solution — VALIDÉ |
| m5 : `Qt6Svg_DIR` d'un autre kit | arrêt « Mélange d'installations Qt » — VALIDÉ |
| m6 : Gmsh et IfcOpenShell absents | configuration réussie, ABSENTE + solution — VALIDÉ |
| m7 : OCCT obligatoire absent (téléchargement désactivé) | arrêt explicite — VALIDÉ |
| Reconfiguration du dossier existant `build-windows-validation` | inchangé (Qt 6.12.0), compilation code 0, avertissements préexistants seulement |
| Suites (`run_test_suites.py`) | **42 réussies, 0 échec, 15 non exécutées** (OpenSees) — identique à la référence |
| Test 491 (sans OpenSees) | nouvelles vérifications réussies |
| Test 37.8 (W-10) | échoue avec l'ancien `Logger.cpp`, réussit avec le correctif |
| `tsa-dependency-report` | rapport, « Tous les chemins retenus existent », OpenSees ABSENT |
| W-8 : déploiement de développement | plus d'avertissement « DLL introuvables : Qt6… » |
| `package_TSA` + `verify_package.py` | 315 fichiers, 78 binaires, fermeture des imports PE — OK |
| Lancement du paquet copié hors du dépôt (chemin avec espace), PATH réduit à System32 | Start Center affiché, Qt6Core / TKernel / TKOpenGl chargés depuis le dossier du paquet, fermeture propre (code 0), journal écrit dans AppData |
| TSALab (dossier `../TSALab/build-windows-validation`, sources TSALab non modifiées) | reconfiguration : mêmes contrôles et même rapport (Qt 6.12.0 et OCCT 8.0.1 VALIDÉES) ; compilation code 0 ; `TSALab_TestSuite` **27/27**, 6 parties ignorées (OpenSees). TEMP de cette session = forme courte 8.3 (ASCII) : **W-6 non exercé, reste ouvert** |

### 11.7 Bloqué, non testé, Linux
- **BLOQUÉ** : OpenSees (W-3) ; Qt 6.11.2 (non installée : non revalidée) ; W-7 (suite torsion, OpenSees).
- **NON TESTÉ** : tout Linux (aucun environnement Linux sur ce poste, WSL sans distribution) — les modifications
  CMake ont été relues pour Linux (Qt système sans chaîne déduite : contrôle ignoré, pas d'échec ; `CMAKE_PREFIX_PATH`
  de `configure.sh` respecté) mais **non exécutées** ; générateur Visual Studio (presets `windows-x64-*`) ; Qt < 6.11
  (6.9, 6.10.3 de la CI : acceptées, NON VALIDÉES) ; configuration Debug ; CI GitHub (aucun push).
- TSALab : `TSALab/CMakeLists.txt` appelle `find_package(Qt6)` avant d'inclure `SetupDependencies.cmake` : il bénéficie
  des contrôles et du rapport, mais pas de la détection automatique de Qt tant qu'il n'appelle pas `tsa_find_qt()`
  (2 lignes, dépôt séparé : **accord requis**).

### 11.8 Décisions (prises par l'utilisateur le 2026-10-28)
1. **Plancher Qt** : conservé à 6.2.
2. **OpenSees** : nouvel exécutable téléchargé → build conda-forge (11.4) ; exécution à valider.
3. **TSALab** : autorisé — `tsa_find_qt()` appelé, presets sans Qt figé, W-6 (correctif déjà préparé dans
   `fix/windows-utf8-test-path`) ; configuration neuve sans Qt6_DIR : Qt 6.12.0 détecté ; compilation code 0.
4. Scripts PowerShell (`setup_build.ps1`, `detect_*.ps1`) : conservés en l'état (documentation corrigée), à
   simplifier plus tard.
5. Commit, fusion et envoi : autorisés.

Incident : l'horloge du poste a reculé d'environ une heure pendant la session ; des fichiers modifiés se sont trouvés
« dans le futur » (Ninja : `build.ninja still dirty`). Seules leurs dates ont été ramenées à l'heure courante.
