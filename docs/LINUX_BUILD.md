# Compiler et tester TSA sous Linux

Vérifié sur Ubuntu 24.04 LTS (x86_64), GCC 13.3, CMake 3.28, Ninja 1.11, Qt 6.12.0 (installateur Qt, `gcc_64`),
OpenCASCADE 8.0.1 compilé depuis les sources. La compilation Windows (presets `ninja-*`, `windows-x64-*`) est inchangée.

## 1. Dépendances

| Dépendance | Version | Source |
|---|---|---|
| GCC/G++, CMake ≥ 3.21, Ninja, Python 3 | GCC 13, CMake 3.28 | `sudo apt install build-essential cmake ninja-build python3` |
| Qt 6 : Core Gui Widgets Svg Network (+ Test) | 6.12.0 vérifié (min. 6.2) | installateur Qt, composant « Desktop gcc 64-bit » |
| OpenCASCADE | **8.0.1** (même version que Windows) | sources officielles, voir §2 |
| En-têtes système d'OCCT | freetype, OpenGL, X11 | `sudo apt install libfreetype-dev libfontconfig-dev libgl-dev libglu1-mesa-dev libx11-dev libxext-dev libxi-dev` |
| Plugin X11 de Qt (lancement de l'interface) | — | `sudo apt install libxcb-cursor0` |
| Dépôts voisins | — | `../TSALab` (obligatoire : cœur scientifique), `../3DDesplacement` (moteur fem3d, facultatif ; Eigen dans `thirdparty/eigen-3.4.1`) |

Les dossiers `opencascade-*-vc14-64/`, `3rdparty-vc14-64/` et `thirdparty/` contiennent des binaires **MSVC** :
ils ne sont jamais utilisés sous Linux (seul Eigen, en-têtes seuls, sert au moteur fem3d).

Non disponibles sous Linux pour l'instant (TSA se construit sans, et le signale) :
- **OpenSees** : aucun exécutable Linux publié ; à compiler depuis les sources (§7) puis placer `OpenSees` dans le `PATH`.
  Sans lui : moteur OpenSees indisponible, suites de tests `opensees`, `extraction`, `engines`… en échec.
- **Gmsh** : l'intégration (`cmake/Gmsh.cmake`) ne prend en charge que le SDK Windows avec MSVC → maillage indisponible.
- **IfcOpenShell** : `-DTSA_IFCOPENSHELL_ROOT=<construction Linux>` ; sinon import IFC par le lecteur STEP interne.

## 2. OpenCASCADE 8.0.1 (installation isolée, sans sudo)

Ubuntu 24.04 ne fournit qu'OCCT 7.6.3, alors que le code est écrit pour 8.0.1. Seuls les modules utilisés sont
compilés (FoundationClasses, ModelingData, ModelingAlgorithms, Visualization). Le RPATH `$ORIGIN` rend les
bibliothèques installées autonomes (sinon leurs dépendances transitives, ex. `libTKShHealing`, sont introuvables).

```bash
mkdir -p ~/build-deps/occt-src && cd ~/build-deps/occt-src
curl -L -o occt-8.0.1.tar.gz https://github.com/Open-Cascade-SAS/OCCT/archive/refs/tags/V8_0_1.tar.gz
tar -xzf occt-8.0.1.tar.gz
cmake -S OCCT-8_0_1 -B ../occt-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$HOME/opt/occt-8.0.1 -DCMAKE_INSTALL_RPATH='$ORIGIN' \
      -DBUILD_LIBRARY_TYPE=Shared -DBUILD_MODULE_ApplicationFramework=OFF -DBUILD_MODULE_DataExchange=OFF \
      -DBUILD_MODULE_Draw=OFF -DUSE_TK=OFF -DUSE_FREETYPE=ON -DUSE_OPENGL=ON -DUSE_XLIB=ON
cmake --build ../occt-build -j "$(nproc)" && cmake --install ../occt-build
```
Durée constatée : ~15 min sur 10 cœurs.

## 3. Compiler et tester

```bash
bash scripts/linux/check-dependencies.sh     # lecture seule : outils, Qt, OCCT, dépôts voisins, OpenSees
bash scripts/linux/configure.sh debug        # preset linux-debug  → build-linux-debug/
bash scripts/linux/build.sh debug            # tout ; ou : build.sh release TSA
bash scripts/linux/test.sh debug             # TSA_TestSuite complet ; ou : test.sh debug --suite=io
```

- Qt et OCCT sont trouvés par `CMAKE_PREFIX_PATH`, construit par les scripts : `TSA_QT_DIR` / `TSA_OCCT_DIR` s'ils sont
  définis, sinon la version la plus récente de `~/Qt/6.*/gcc_64` et `~/opt/occt-8.*`. Aucun chemin propre à une machine
  n'est versionné. Sans les scripts : `CMAKE_PREFIX_PATH=~/opt/occt-8.0.1:~/Qt/6.12.0/gcc_64 cmake --preset linux-debug`.
- `TSA_JOBS` fixe le parallélisme de `build.sh` (défaut : `nproc`).
- Les scripts sont lancés par `bash` : le bit exécutable n'est pas garanti (volume NTFS, extraction Windows) ;
  `.gitattributes` impose LF pour `*.sh`.
- `test.sh` lance les tests depuis le dossier de build (fichiers temporaires dans `build-linux-*/build/test_tsa_data`,
  aucun fichier suivi par Git n'est réécrit). Plateforme Qt : `xcb` dans une session X11 où le plugin xcb de Qt est
  utilisable (vues OCCT réelles, comme `windows` sous Windows ; des fenêtres s'ouvrent brièvement), sinon `offscreen` :
  les tests de vue 3D protégés sont `[SKIP]`, les autres arrêtent le programme (`BadWindow`, BUG-076). Forcer :
  `QT_QPA_PLATFORM=offscreen bash scripts/linux/test.sh debug`.
- Test 457 (recherche < 50 ms) : seuil atteint en Release (11 ms), dépassé en Debug non optimisé (~75 ms).
- `TSA_TestSuite` s'arrête au premier échec de chaque suite : les tests suivants de la suite ne sont pas comptés.

## 4. Lancer l'application

```bash
build-linux-debug/TSA
```
Exige une session graphique X11 et `libxcb-cursor0`. Sous Wayland, Qt passe par XWayland (`QT_QPA_PLATFORM=xcb`),
le viewport OCCT utilisant `Xw_Window` (X11). Vérifié le 24/10/2026 : Start Center, ouverture d'un projet (pont treillis,
253 nœuds / 665 barres), vue 3D rendue. Sur un portable hybride, le rendu passe par défaut par le GPU intégré (Mesa) ;
pour le GPU NVIDIA : `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia build-linux-debug/TSA` (non testé).

## 5. Différences Windows / Linux traitées

- **Locale numérique** : sous Unix, `QCoreApplication` applique la locale du système (`setlocale(LC_ALL, "")`) ; avec
  une locale à virgule (fr_FR), `std::stod`/`strtod` lisaient « 3.5 » comme 3 et `std::to_string` écrivait « 0,5 »
  (fichiers d'échange, résultats OpenSees, STEP/IFC, scripts). `src/Core/NumericLocale.cpp` rétablit `LC_NUMERIC="C"`
  dans le constructeur de toute `QCoreApplication` (comportement identique à Windows). Test 490.
- `cmake/SetupDependencies.cmake` ne télécharge le SDK OCCT Windows que sous Windows.
- `cmake/IfcOpenShell.cmake` : pas de chemin par défaut vers la construction MSVC hors Windows.
- `OccView` : `Xw_Window` reçoit un `Aspect_Drawable` (OCCT ≥ 7.6 n'expose plus le type X11 `Window`).
- Extension de miniatures de l'Explorateur (`TSAThumbnailProvider`, test 128) : Windows uniquement.
- Textures des matériaux : OCCT décode les fichiers image avec WIC sous Windows, avec FreeImage ailleurs ; sans
  FreeImage, `MaterialVisual` les décode avec Qt (sinon « no image library available » et texture absente).
- Modules : `command: "bin/x.exe"` résolu en `bin/x` hors Windows (docs/SDK.md, BUG-094).
- Réglages (`QSettings`) : registre sous Windows, fichier INI `~/.config/<organisation>/<application>.conf` sous Linux ;
  chemins de documents sensibles à la casse sous Linux (`DisplayProfile`).

## 6. Avertissements connus (GCC)

Dépréciations OCCT 8 (`Standard_True`, `AIS_ListOfInteractive`, `TColStd_IndexedDataMapOfStringString`,
`AIS_InteractiveContext::DetectedCurrentObject`) : non bloquantes, à moderniser séparément.

## 7. OpenSees 3.8.0 pour Linux (plan, non exécuté)

Aucun binaire Linux n'est publié. Le CMake d'OpenSees 3.8.0 exige un compilateur Fortran et trouve par `find_package`
ZLIB, HDF5, Tcl 8.6 et Eigen3 (obligatoires), LAPACK/BLAS (sinon copies internes) ; MPI, MKL, MUMPS facultatifs.
La méthode officielle passe par Conan (paquets hdf5, tcl, zlib, eigen) ; avec les paquets Ubuntu :

```bash
sudo apt install gfortran libhdf5-dev tcl8.6-dev libeigen3-dev liblapack-dev   # zlib1g-dev déjà présent
cd ~/build-deps/opensees-src-dl
curl -L -o os-3.8.0.tar.gz https://github.com/OpenSees/OpenSees/archive/refs/tags/v3.8.0.tar.gz && tar -xzf os-3.8.0.tar.gz
cmake -S OpenSees-3.8.0 -B ../opensees-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build ../opensees-build --target OpenSees -j "$(nproc)"
# Emplacement du binaire et copie de lib/tcl8.6 (init.tcl) à confirmer après compilation : le CMake d'OpenSees
# cherche ${TCL_INCLUDE_PATH}/../lib/tcl8.6, absent avec Tcl d'Ubuntu (/usr/share/tcltk/tcl8.6, trouvé par libtcl).
install -D "$(find ../opensees-build -maxdepth 3 -type f -name OpenSees -perm -u+x | head -n 1)" ~/opt/opensees-3.8.0/bin/OpenSees
export PATH="$HOME/opt/opensees-3.8.0/bin:$PATH"
```
Validation avant usage : `echo 'puts [package require OpenSees]' | OpenSees` (version), un modèle minimal (console
élastique, flèche PL³/3EI), code de retour 0, puis `bash scripts/linux/test.sh release --suite=extraction` (comparaison
aux références OpenSees 3.8.0 de la suite). `thirdparty/OpenSees` (Windows) n'est pas modifié.

## 8. Valider la compilation Windows (MSVC)

Sous Windows, dans « x64 Native Tools Command Prompt for VS » :
```bat
cmake --preset ninja-debug
cmake --build --preset ninja-debug -- -k 0 -j 4
build-ninja-debug\TSA_TestSuite.exe
cmake --preset ninja-release && cmake --build --preset ninja-release -- -j 4 && build-ninja-release\TSA_TestSuite.exe
python tools\check_layers.py src && python tools\i18n\check_translations.py --strict
```
Attendu : compilation sans erreur ni nouvel avertissement, suite complète réussie (471 tests attendus, dont le test 128
exécuté sous Windows), puis TSALab (`AGENTS.md`). Points à surveiller : `NumericLocale.cpp` (Q_COREAPP_STARTUP_FUNCTION),
`ConverterSpec::executable`, chemins `#ifdef Q_OS_WIN` de `OpenSeesManager`, `MaterialVisual`, tests 274/279/387.

## 9. Commit depuis Linux (fins de ligne)

La copie de travail est partagée avec Windows et écrite en CRLF, l'index est en LF, `core.autocrlf` n'est pas défini.
Un `git add` simple enregistrerait les fichiers CRLF tels quels. Ajouter avec normalisation, sans modifier les fichiers :
```bash
git -c core.autocrlf=input add <fichiers>
git diff --cached --stat     # ne doit montrer que les vrais changements
```
