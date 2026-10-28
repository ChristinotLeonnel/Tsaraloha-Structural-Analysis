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

Facultatives (TSA se construit sans, et le signale) ; installées hors du dépôt, dans `~/opt` :
- **OpenSees 3.8.0** : aucun exécutable Linux publié ; compilation vérifiée (§7). Sans lui : moteur OpenSees
  indisponible, suites `opensees`, `extraction`, `engines`… en échec.
- **Gmsh 4.15.2** : SDK Linux officiel (§10), même version que le SDK Windows. Sans lui : maillage indisponible, suites
  `mesh`, `meshview`, `surfaces`, `benchmarks` en échec.
- **IfcOpenShell 0.9.0post1** : `scripts/linux/build-ifcopenshell.sh` (§11). Sans lui : import IFC par le lecteur STEP
  interne, test 400 en échec.

## 2. OpenCASCADE 8.0.1 (installation isolée, sans sudo)

Ubuntu 24.04 ne fournit qu'OCCT 7.6.3, alors que le code est écrit pour 8.0.1. TSA utilise FoundationClasses,
ModelingData, ModelingAlgorithms et Visualization ; IfcOpenShell (§11) lie en plus DataExchange (`TKXSBase`, `TKDESTEP`,
`TKDEIGES`…) et ApplicationFramework (`TKBin`) : les compiler dès le départ si l'import IFC complet est voulu. Ajoutés
après coup (`-DBUILD_MODULE_DataExchange=ON -DBUILD_MODULE_ApplicationFramework=ON` sur la même construction), les
bibliothèques déjà installées restent identiques octet pour octet (constaté le 25/10/2026). Le RPATH `$ORIGIN` rend les
bibliothèques installées autonomes (sinon leurs dépendances transitives, ex. `libTKShHealing`, sont introuvables).

```bash
mkdir -p ~/build-deps/occt-src && cd ~/build-deps/occt-src
curl -L -o occt-8.0.1.tar.gz https://github.com/Open-Cascade-SAS/OCCT/archive/refs/tags/V8_0_1.tar.gz
tar -xzf occt-8.0.1.tar.gz
cmake -S OCCT-8_0_1 -B ../occt-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$HOME/opt/occt-8.0.1 -DCMAKE_INSTALL_RPATH='$ORIGIN' \
      -DBUILD_LIBRARY_TYPE=Shared -DBUILD_MODULE_ApplicationFramework=ON -DBUILD_MODULE_DataExchange=ON \
      -DBUILD_MODULE_Draw=OFF -DUSE_TK=OFF -DUSE_FREETYPE=ON -DUSE_OPENGL=ON -DUSE_XLIB=ON
cmake --build ../occt-build -j "$(nproc)" && cmake --install ../occt-build
```
Durée constatée : ~15 min sur 10 cœurs pour les 4 modules de TSA ; ajout de DataExchange et
ApplicationFramework sur la construction existante : ~15 min de plus (3 014 étapes, `-j 6`).

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
- `cmake/SetupDependencies.cmake` ne télécharge le SDK OCCT Windows que sous Windows. Ses contrôles s'appliquent aussi
  sous Linux (Qt de chaîne ELF, OCCT ≥ 8.0, aucun module Qt d'une autre installation) ; sans Qt dans
  `CMAKE_PREFIX_PATH`, la plus récente de `~/Qt/6.*/gcc_64` est retenue ; rapport `<build>/tsa-dependencies.txt` et
  cible `tsa-dependency-report` (`docs/DEPENDENCIES.md`). **Non vérifié sous Linux** à ce jour (2026-10-28).
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

## 7. OpenSees 3.8.0 pour Linux

Aucun binaire Linux n'est publié. Procédure vérifiée le 24/10/2026 (Ubuntu 24.04, GCC/gfortran 13) : méthode officielle
d'OpenSees, **Conan 2** pour Tcl 8.6.11, HDF5 1.14, zlib et Eigen ; gfortran et LAPACK du système. Installation isolée,
sans sudo une fois les paquets présents ; `thirdparty/OpenSees` (Windows) n'est pas modifié.

Le mode « paquets système » (sans Conan) ne fonctionne pas avec OpenSees 3.8.0 : `find_package(TCL REQUIRED)` exige
aussi Tk (non utilisé par OpenSees) et la bibliothèque Tcl est copiée depuis `${TCL_INCLUDE_PATH}/../lib/tcl8.6`, absent
sous Ubuntu.

```bash
sudo apt install gfortran liblapack-dev                       # compilateur Fortran exigé par le CMake d'OpenSees
mkdir -p ~/build-deps/opensees-src && cd ~/build-deps/opensees-src
curl -L -o os-3.8.0.tar.gz https://github.com/OpenSees/OpenSees/archive/refs/tags/v3.8.0.tar.gz && tar -xzf os-3.8.0.tar.gz
cd ~/build-deps
python3 -m venv conan-venv && ./conan-venv/bin/pip install "conan>=2,<3" && ./conan-venv/bin/conan profile detect
./conan-venv/bin/conan install opensees-src/OpenSees-3.8.0 --output-folder=opensees-conan --build=missing -s build_type=Release
# SRC/api/CMakeLists.txt : hors Windows, OPS_Api ne contient qu'un en-tête (langage d'édition de liens indéterminé).
cat > opensees-linux-fixes.cmake <<'FIX'
include_guard(GLOBAL)
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL set_target_properties OPS_Api PROPERTIES LINKER_LANGUAGE CXX)
FIX
cmake -S opensees-src/OpenSees-3.8.0 -B opensees-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$HOME/build-deps/opensees-conan/build/Release/generators/conan_toolchain.cmake \
      -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_PROJECT_INCLUDE=$HOME/build-deps/opensees-linux-fixes.cmake
cmake --build opensees-build --target OpenSees -j "$(nproc)"       # ~1 800 fichiers
# L'interpréteur cherche sa bibliothèque Tcl dans <dossier de l'exécutable>/../lib/tcl8.6
install -D -m 755 opensees-build/OpenSees ~/opt/opensees-3.8.0/bin/OpenSees
cp -r ~/.conan2/p/b/tcl*/p/lib/tcl8.6 ~/opt/opensees-3.8.0/lib/
```

Résolution du chemin (`OpenSeesManager::findExecutableInternal`), dans cet ordre :
1. chemin enregistré `OpenSees/ExecutablePath` de `~/.config/TSA/TSA_StructuralAnalysis.conf` (s'il existe ; aucune
   option de l'interface ne l'écrit aujourd'hui, à renseigner dans le fichier) :
   ```ini
   [OpenSees]
   ExecutablePath=/home/<utilisateur>/opt/opensees-3.8.0/bin/OpenSees
   ```
2. `bin/OpenSees` ou `OpenSees` du dossier OpenSees de l'application (`engines/` ou `thirdparty/OpenSees`) ;
3. `OpenSees` puis `gOpenSees` dans le `PATH`.

Le `PATH` d'une application lancée depuis le bureau est celui de la session (lu dans `~/.profile` à la connexion) :
sans le chemin enregistré, ajouter `export PATH="$HOME/opt/opensees-3.8.0/bin:$PATH"` à `~/.profile`, puis se
reconnecter. **Pas de lien symbolique** (ex. `~/.local/bin/OpenSees`) : l'interpréteur cherche `lib/tcl8.6` à côté
du lien, non de la cible (constaté : « Can't find a usable init.tcl »).

Vérification (`OpenSeesManager::checkExecutable`, BUG-099) : l'exécutable reçoit sur l'entrée standard
`wipe; model BasicBuilder -ndm 2 -ndf 2; puts [format "OPENSEES_CHECK_OK_%s" [version]]; exit` et n'est retenu que si
`OPENSEES_CHECK_OK_<version>` revient, avec une version ≥ 3.4.0. OpenSees 3.8.0 n'a pas d'option de version (seules
`-noHeader` et `-suppressOutput` existent), écrit tout sur la sortie d'erreur et rend 0 même quand Tcl ne s'initialise
pas : la bannière ne prouve rien. Diagnostics distincts : introuvable, lancement impossible, délai dépassé, Tcl non
initialisé, version incompatible, réponse inattendue (test 491, sans installation requise).
```bash
echo 'puts [format "OPENSEES_CHECK_OK_%s" [version]]; exit' | ~/opt/opensees-3.8.0/bin/OpenSees   # → OPENSEES_CHECK_OK_3.8.0
```
Validation constatée : console 2D (PL³/3EI, PL²/2EI, réactions) et poutre 3D bi-appuyée (5qL⁴/384EI) exactes à la
précision machine ; suite `opensees` 17/17 et suite `extraction` réussies.

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

## 10. Gmsh 4.15.2 (maillage)

SDK Linux officiel, même version que `thirdparty/gmsh-4.15.2-Windows64-sdk` ; aucune compilation, aucun sudo :
```bash
mkdir -p ~/build-deps/gmsh-dl && cd ~/build-deps/gmsh-dl
curl -fsSLO https://gmsh.info/bin/Linux/gmsh-4.15.2-Linux64-sdk.tgz
sha256sum gmsh-4.15.2-Linux64-sdk.tgz   # constaté le 25/10/2026 : 2dbd68d033f99b05789554bf4db6d47f4108dd36b0b36d4a50935df0ceb9e772
tar -xzf gmsh-4.15.2-Linux64-sdk.tgz -C ~/opt
```
`scripts/linux/configure.sh` le trouve (`TSA_GMSH_SDK`, sinon `~/opt/gmsh-*-Linux64-sdk`) et passe `-DTSA_GMSH_SDK`.
`libgmsh.so` est liée directement (RUNPATH de l'arborescence de compilation).

Deux pièges propres à Linux, corrigés dans `cmake/Gmsh.cmake` et `GmshMeshEngine.cpp` :
- **API native `gmsh.h`, pas `gmsh.h_cwrap`** : `libgmsh.so` exporte son API C++ sous les mêmes noms décorés que les
  fonctions en ligne de `gmsh.h_cwrap` ; le chargeur ELF résolvait les appels internes de Gmsh vers ces copies, qui
  rappellent l'API C → récursion infinie (13 Go consommés au premier maillage 1D, processus tué par le noyau).
  Windows n'est pas concerné (pas d'interposition entre DLL) et garde `gmsh.h_cwrap`.
- **Instanciations `std` non exportées** (`cmake/gmsh-hide-std.map`) : le SDK est compilé avec GCC 6.5 ; un exécutable
  compilé avec GCC 13 exportait ses instanciations de `std::regex`, `std::function`… que le chargeur substituait à
  celles de Gmsh (disposition mémoire différente) → corruption, constatée sur les noms de groupes physiques (tests 293,
  340). Le script de version, appliqué aux seuls exécutables liés à Gmsh, les garde locales ; contrôle :
  `nm -D --defined-only build-linux-release/TSA | grep -cE ' [TW] _Z(N|NK|S)St'` → 0. Diagnostic :
  `LD_DEBUG=bindings LD_DEBUG_OUTPUT=/tmp/b build-linux-release/TSA_TestSuite --suite=meshview`, puis chercher les
  liaisons `binding file …libgmsh… to ./TSA_TestSuite`.

Vérification en ligne de commande : `~/opt/gmsh-4.15.2-Linux64-sdk/bin/gmsh carre.geo -2 -format msh4 -o carre.msh`.

## 11. IfcOpenShell 0.9.0post1 (import IFC complet)

Même recette, mêmes sources et mêmes empreintes que `scripts/build_ifcopenshell.ps1` (Windows) : IfcOpenShell
0.9.0post1, Boost 1.92.0 (`program_options`, `regex`, statiques), Eigen 3.4.1, OCCT 8.0.1 de TSA **avec DataExchange et
ApplicationFramework** (§2 ; sinon l'édition de liens échoue sur `-lTKXSBase -lTKBin -lTKDESTEP -lTKDEIGES`).
```bash
bash scripts/linux/build-ifcopenshell.sh        # → ~/opt/ifcopenshell-0.9.0post1 ; travail dans ~/build-deps/ifcopenshell
bash scripts/linux/configure.sh release         # trouve ~/opt/ifcopenshell-* → -DTSA_IFCOPENSHELL_ROOT
```
Les archives de `thirdparty/_build-ifcopenshell/dl` (construction Windows) sont réutilisées si leur empreinte
correspond. Boost et Eigen sont installés dans le préfixe d'IfcOpenShell ; les 13 bibliothèques et modules (schémas
IFC2X3, IFC4, IFC4X3_ADD2, noyau OpenCASCADE, correspondances) restent dans `<préfixe>/lib`, où `libifcopenshell.parse`
les cherche (`dladdr`) : aucune copie à côté de TSA (RUNPATH). Durée non mesurée (construction interrompue puis reprise) ;
`-j 4` par défaut (`TSA_JOBS`) : les schémas IFC sont très gourmands en mémoire.

**`-Wl,-Bsymbolic` (dans le script)** : sans lui, les instanciations `std` et Boost des modules IfcOpenShell (GCC 13)
étaient résolues vers celles de `libgmsh.so` (GCC 6, chargée avant les modules) → erreur de segmentation dans
`faceset_helper` (`std::map`) dès le premier fichier IFC (test 400). Contrôle : aucune liaison `binding file
…ifcopenshell… to …libgmsh…` sous `LD_DEBUG=bindings`.

Piège rencontré : `tsa_find_occt` retient le plus récent des `~/opt/occt-8.*` (tri par version) ; une sauvegarde nommée
`~/opt/occt-8.0.1.<suffixe>` serait choisie à la place d'OCCT. Nommer les sauvegardes hors de ce motif.
