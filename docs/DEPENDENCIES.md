# Dépendances : détection, versions, diagnostic

Référence technique de la gestion des dépendances de TSA (et de TSALab, qui utilise les mêmes modules CMake).
Code : `cmake/SetupDependencies.cmake` (module central), `cmake/Gmsh.cmake`, `cmake/IfcOpenShell.cmake`,
`cmake/RuntimeDependencies.cmake`, `cmake/DeployDependencies.cmake`, `cmake/PackageTSA.cmake`,
`cmake/DependencyReport.cmake`. Licences : `docs/THIRD_PARTY_LICENSES.md`. Linux : `docs/LINUX_BUILD.md`.

**Principe** : toute version *déclarée compatible* est acceptée, mais seules les versions réellement compilées et
testées sont dites **VALIDÉES**. Une version acceptée sans essai est affichée **NON VALIDÉE**. Une version ou une
chaîne de compilation incompatible arrête la configuration avec la cause et la solution.

## 1. Inventaire

| Dépendance | Rôle | Obligatoire | Mécanisme | Versions acceptées | Validées |
|---|---|---|---|---|---|
| Qt 6 (Core, Gui, Widgets, Svg, Network) | interface, noyau | oui | `find_package(Qt6)` (cibles `Qt6::*`) | ≥ 6.2, < 7 | 6.11.2, 6.12.0 |
| Qt Test | suite « shortcuts » | non | `find_package(Qt6 QUIET COMPONENTS Test)` | idem | idem |
| Qt LinguistTools | traductions en / mg | non | `qt_add_lrelease` | ≥ 6.9 (sinon désactivées) | 6.12.0 |
| OpenCASCADE | géométrie, viewport | oui | `find_package(OpenCASCADE)` ; SDK Windows téléchargé | ≥ 8.0 | 8.0.1 |
| 3rdparty OCCT (FreeType, FreeImage, TBB…) | DLL d'exécution d'OCCT | oui (Windows) | dossier `3rdparty-vc14-64`, téléchargé | celles du SDK OCCT | 8.0.1 |
| Gmsh (SDK) | maillage | non | `cmake/Gmsh.cmake` → `TSA::Gmsh` | SDK officiel avec `gmsh.h_cwrap` | 4.15.2 |
| IfcOpenShell (+ Boost, Eigen) | import IFC complet | non | `find_package(IfcOpenShell CONFIG)` | construit contre l'OCCT de TSA | 0.9.0post1 |
| OpenSees | moteur de calcul externe | non | exécutable (`OpenSeesManager`), Tcl 8.6 ; Windows : conda-forge épinglé (§5) | ≥ 3.4.0 (contrôle au lancement) | 3.8.0 archive officielle (antérieurement) ; build conda-forge : à valider |
| windeployqt, dumpbin | déploiement Windows | non (dév.) | `bin/` de la Qt retenue ; invite x64 Native Tools | — | — |
| Python 3 | bibliothèque d'importation Gmsh, contrôles CTest | non | `find_package(Python3)` | 3.x | — |

**VTK n'est pas une dépendance de TSA** : il fait partie de l'archive 3rdparty du SDK OCCT, n'est ni lié ni déployé
(`verify_package.py` le refuse dans le paquet). `scripts/detect_dependencies.ps1` l'affiche encore à titre indicatif.

## 2. Justification des versions

### Qt
- **Plancher 6.2** (inchangé) : recherche dans `src/`, `tests/`, `tools/` et `sdk/` des API C++ apparues de 6.3 à 6.10
  (`Qt::ColorScheme`, `QChronoTimer`, `QDirListing`, `QSpan`, `QLatin1StringView`, littéraux `_s`/`_L1`,
  `QPainterStateGuard`, `QRangeModel`, `readLineInto`, `hashInto`, `QTimeZone::UTC`, etc.) : aucune trouvée ; aucun
  test de version (`QT_VERSION_CHECK`) dans le code. Ce plancher n'est pas vérifié par compilation (aucune Qt
  < 6.10 disponible) : ces versions sont acceptées mais NON VALIDÉES.
- **Traductions ≥ 6.9** : `qt_add_lrelease(TS_FILES … LRELEASE_TARGET … QM_OUTPUT_DIRECTORY …)` — forme sans cible
  depuis Qt 6.7, `QM_OUTPUT_DIRECTORY` depuis Qt 6.9 (documentation de `qt_add_lrelease`). Avec une Qt plus ancienne,
  la configuration désactive les traductions et l'annonce (avant : échec ou ressources `.qm` mal placées).
- **Validées** : 6.12.0 (Windows MSVC 19.51, 2026-10-28 ; Linux GCC 13, `docs/LINUX_BUILD.md`), 6.11.2 (poste de
  développement Windows jusqu'au 2026-10, historique du projet). La CI Windows épingle 6.10.3 (aqtinstall ne lit pas
  6.11+) : NON VALIDÉE tant que la CI n'a pas tourné.
- **Chaîne de compilation** : un kit Qt MSVC (`Qt6Core.lib`) avec MSVC, MinGW (`libQt6Core.a`) avec GCC sous
  Windows, ELF ailleurs. Kit `msvcAAAA` : Visual Studio AAAA ou plus récent (compatibilité binaire MSVC :
  le toolset doit être au moins aussi récent que celui des bibliothèques). Windows : x64 seulement.

### OpenCASCADE
- **≥ 8.0** : le code utilise l'API 8 (`Xw_Window` sur `Aspect_Drawable`, dépréciations 8.0 traitées) ; OCCT 7.6
  (Ubuntu 24.04) ne convient pas (`docs/LINUX_BUILD.md`). Le fichier de version du SDK n'accepte que la version
  exacte : TSA appelle `find_package(OpenCASCADE)` sans version puis contrôle lui-même la plage.
- **Windows** : SDK officiel `opencascade-8.0.1-vc14-64` (x64, lié avec MSVC 14.44, `OpenCASCADE_COMPILER=vc14`) :
  MSVC obligatoire ; un compilateur MinGW est refusé avec explication.

### Gmsh, IfcOpenShell
Versions validées 4.15.2 et 0.9.0post1 (les seules installées et testées). Une autre version trouvée est utilisée et
signalée NON VALIDÉE.

### OpenSees
Version ≥ 3.4.0 exigée au lancement (`OpenSeesManager::minimumSupportedVersion`, commande `version` d'OpenSees).

## 3. Détection et sélection

### Qt (`tsa_find_qt`, avant `find_package(Qt6)`)
1. `Qt6_DIR` valide (contient `Qt6Config.cmake`) : utilisé tel quel.
2. Une Qt dans `CMAKE_PREFIX_PATH` (variable CMake ou d'environnement) : laissée à `find_package`.
3. Sinon : candidats des variables d'environnement `Qt6_DIR`, `QTDIR`, `TSA_QT_DIR`, puis `<racine>/6.*/<kit>`
   pour chaque racine de `TSA_QT_SEARCH_ROOTS` (Windows : `C:/Qt`, `%SystemDrive%/Qt`, `%USERPROFILE%/Qt` ;
   Linux : `~/Qt`). Retenue : la **version la plus récente ≥ 6.2 de la chaîne de compilation du compilateur**.
   Kits Android, WebAssembly, iOS, ARM64 ignorés. La liste des installations examinées est affichée.
4. Un `Qt6_DIR` en cache vers une Qt disparue (ex. presets anciens en 6.11.2) est remplacé ; les autres `Qt6*_DIR`
   du cache hors de la Qt retenue sont effacés.

Les presets n'imposent plus de chemin Qt. Pour forcer une Qt : `-DQt6_DIR=<Qt>/<version>/<kit>/lib/cmake/Qt6`,
`-DCMAKE_PREFIX_PATH=<Qt>/<version>/<kit>` ou `-DTSA_QT_SEARCH_ROOTS=<dossier>`.

### Contrôles après `find_package` (`tsa_validate_dependencies`, appelé par `TSAProduct.cmake`)
Architecture x64 (Windows) ; chaîne de compilation Qt et OCCT ; année du kit MSVC ; **mélange** (un module Qt
trouvé dans une autre installation que `Qt6_DIR` → erreur) ; plages de versions. Appelé pour TSA et pour TSALab.

### OpenCASCADE, 3rdparty
Windows : `OpenCASCADE_DIR` s'il est valide, sinon `opencascade-<TSA_OCCT_VERSION>-vc14-64` de la base commune, sinon
téléchargement (GitHub Releases OCCT, si `TSA_AUTO_DOWNLOAD_DEPS=ON`). Linux : `find_package` (préfixe donné par
`scripts/linux/configure.sh`). Les DLL déployées viennent de `OpenCASCADE_BINARY_DIR` de l'OCCT retenu (plus du
chemin `opencascade-8.0.1-vc14-64` figé).

### Déploiement Windows
- `windeployqt` : celui du `bin/` de la Qt retenue (`tsa_find_windeployqt`), jamais un `windeployqt` d'une autre
  installation resté en cache.
- DLL non Qt : `dumpbin` + `file(GET_RUNTIME_DEPENDENCIES)` (seules les DLL importées sont copiées ; copies d'une
  même DLL comparées par SHA-256 : deux versions différentes → erreur).
- Développement (`DeployDependencies.cmake`) : les DLL Qt ne sont pas signalées « introuvables » (W-8).
  Paquet (`PackageTSA.cmake`) : une DLL Qt non résolue reste une erreur ; `tools/verify_package.py` contrôle en plus
  la fermeture des imports PE de chaque binaire.

### Linux
`scripts/linux/configure.sh` : `TSA_QT_DIR` / `TSA_OCCT_DIR` / `TSA_GMSH_SDK` / `TSA_IFCOPENSHELL_ROOT`, sinon la
version la plus récente de `~/Qt/6.*/gcc_64`, `~/opt/occt-8.*`… (`scripts/linux/common.sh`). Les contrôles de
`tsa_validate_dependencies` s'appliquent aussi (chaîne ELF). Bibliothèques partagées : RUNPATH, aucune copie
(`docs/LINUX_BUILD.md`).

## 4. Diagnostic

- À chaque configuration : bloc « Dépendances retenues » et fichier `<build>/tsa-dependencies.txt` (plateforme,
  compilateur, CMake, puis pour chaque dépendance : statut, version, chemin, note).
- `cmake --build <build> --target tsa-dependency-report` : réaffiche ce rapport, vérifie que chaque chemin retenu
  existe toujours et cherche OpenSees comme l'application le fera. Lecture seule : rien n'est installé ni modifié.

Statuts : VALIDÉE, NON VALIDÉE, TROUVÉE (outil sans plage de versions), ABSENTE (facultative), DÉSACTIVÉE (option).

## 5. OpenSees (W-3)

- **Constat (2026-10-28)** : l'archive officielle `OpenSees3.8.0-x64.exe.zip`
  (`opensees.berkeley.edu/OpenSees/code/…`) n'est plus servie : le site redirige vers
  `peer.berkeley.edu/openseesOpenSees/…` (barre oblique manquante), réponse 404 ; la page de téléchargement aussi.
  Les publications GitHub `OpenSees/OpenSees` (v3.8.0, 2026-02-18) ne contiennent que le code source ; le manifeste
  winget `OpenSees.OpenSees` 3.8.0 désigne la même adresse (donc winget échoue aussi) ; l'archive n'est pas conservée
  par la Wayback Machine (pas plus que l'ancien miroir `opensees.ist.berkeley.edu`). **Aucune source officielle de
  l'archive Windows n'est joignable.**
- **Source retenue (Windows) : conda-forge**, `opensees 3.8.0` win-64, compilé depuis l'étiquette officielle
  `v3.8.0` (`github.com/OpenSees/OpenSees/archive/refs/tags/v3.8.0.tar.gz`, SHA-256 fixé dans la recette) par la
  recette publique `github.com/conda-forge/opensees-feedstock`, et contrôlé par sa CI (présence de
  `Library\bin\OpenSees.exe`). `cmake/SetupDependencies.cmake` (`TSA_OPENSEES_CONDA_PACKAGES`) épingle 12 paquets
  (build exacte + SHA-256 publiés par anaconda.org) : `opensees`, `tk` (tcl86t.dll et bibliothèque Tcl 8.6), `hdf5`,
  `libblas` / `liblapack` (variante OpenBLAS, DLL autonomes), `libaec`, `libcurl`, `libzlib`, `openssl`, `libssh2`,
  `libpsl`, `icu`. Seuls `OpenSees.exe`, les 13 DLL de sa fermeture d'imports (dumpbin, 2026-10-28) et `lib/tcl8.6`,
  `lib/tcl8` sont installés dans `thirdparty/OpenSees` (≈ 70 Mo), avec `SOURCE.txt` (provenance). Un paquet
  d'empreinte différente est rejeté et rien n'est installé (assemblage dans un dossier temporaire, copie à la fin).
  Aucun exécutable téléchargé n'est lancé par la configuration. Choix : builds publiées depuis plus de 14 jours.
- **Archive officielle** : SHA-256 `5E37A5A9…FB41` du manifeste winget, dans `TSA_OPENSEES_SHA256` (CMake) et
  `OpenSeesManager::officialArchiveSha256()` (application) ; le test 491 vérifie que les deux sont identiques.
  `-DTSA_OPENSEES_ARCHIVE=<OpenSees3.8.0-x64.exe.zip>` l'installe à la place de conda-forge.
- **Ordre de recherche** : `thirdparty/OpenSees` (`bin/OpenSees.exe` + `lib/tcl8.6`), PATH, chemin enregistré dans
  les préférences ; sinon installation ci-dessus (si `TSA_AUTO_DOWNLOAD_DEPS=ON`). Le téléchargement intégré à
  l'application (`OpenSeesManager::downloadAndInstall`) vise encore l'archive officielle : il échoue proprement
  (404, message explicite) tant qu'elle n'est pas republiée.
- **Différences avec l'archive officielle** : BLAS/LAPACK OpenBLAS (au lieu de la build officielle), HDF5 1.14.6 ;
  les résultats doivent être confirmés par les suites OpenSees et le banc (références externes) avant d'être tenus
  pour validés.
- **Linux** : compilation depuis `v3.8.0` (`docs/LINUX_BUILD.md` §7) ; conda-forge publie aussi `linux-64` (non
  intégré).
- **Sans OpenSees** : TSA se construit et fonctionne (moteur Custom2D, modélisation, IFC, maillage…) ;
  `tools/ci/run_test_suites.py` marque les suites qui en dépendent NON EXÉCUTÉES (jamais réussies).
- **Licence** : OpenSees n'est pas inclus dans le paquet par défaut (`TSA_PACKAGE_OPENSEES=OFF`, licence UC :
  usage éducatif / recherche, diffusion commerciale soumise à accord) — `docs/THIRD_PARTY_LICENSES.md`.
