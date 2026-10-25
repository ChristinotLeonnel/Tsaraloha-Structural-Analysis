#!/usr/bin/env bash
# Compile IfcOpenShell 0.9.0post1 (lecture IFC et noyau géométrique OpenCASCADE) contre l'OCCT 8 de TSA sous Linux,
# avec Boost 1.92.0 (program_options, regex, statiques) et Eigen 3.4.1 (en-têtes) : même recette, mêmes sources et
# mêmes empreintes que scripts/build_ifcopenshell.ps1 (Windows). Tout est installé dans un seul préfixe, à passer à
# CMake par -DTSA_IFCOPENSHELL_ROOT (scripts/linux/configure.sh le trouve dans ~/opt/ifcopenshell-*).
#
# Usage : bash scripts/linux/build-ifcopenshell.sh [préfixe (défaut ~/opt/ifcopenshell-0.9.0post1)] [dossier de travail]
# Variables : TSA_OCCT_DIR (sinon ~/opt/occt-8.*), TSA_JOBS (défaut 4 : schémas IFC très gourmands en mémoire).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

prefix="${1:-$HOME/opt/ifcopenshell-0.9.0post1}"
work="${2:-$HOME/build-deps/ifcopenshell}"
jobs="${TSA_JOBS:-4}"
occt="$(tsa_find_occt)"
[[ -f "$occt/include/opencascade/Standard_Version.hxx" ]] || { tsa_error "OCCT 8 introuvable (TSA_OCCT_DIR)"; exit 1; }

# nom|archive|dossier extrait|url|sha256
sources=(
  "boost|boost-1.92.0.tar.gz|boost-1.92.0|https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-b2-nodocs.tar.gz|e2a814b3a158ab482c7a3d330f8bf5a7a8423d258a4e2fb396996e00fcee2111"
  "eigen|eigen-3.4.1.tar.gz|eigen-3.4.1|https://gitlab.com/libeigen/eigen/-/archive/3.4.1/eigen-3.4.1.tar.gz|b93c667d1b69265cdb4d9f30ec21f8facbbe8b307cf34c0b9942834c6d4fdbe2"
  "ifcopenshell|ifcopenshell-0.9.0post1.tar.gz|IfcOpenShell-ifcopenshell-python-0.9.0post1|https://github.com/IfcOpenShell/IfcOpenShell/archive/refs/tags/ifcopenshell-python-0.9.0post1.tar.gz|2776eef7ddc0137df317f0294623523dc4ccdaba468c172232cfdda13f4e912a"
)
# Archives déjà téléchargées par la construction Windows (même empreinte) : réutilisées.
shared_dl="$TSA_SOURCE_DIR/thirdparty/_build-ifcopenshell/dl"

mkdir -p "$work/dl" "$work/src" "$prefix"
for s in "${sources[@]}"; do
    IFS='|' read -r name file dir url sha <<< "$s"
    archive="$work/dl/$file"
    if [[ ! -f "$archive" ]]; then
        if [[ -f "$shared_dl/$file" ]]; then cp "$shared_dl/$file" "$archive"
        else tsa_info "Téléchargement $url"; curl -fsSL -o "$archive" "$url"; fi
    fi
    if [[ "$(sha256sum "$archive" | cut -d' ' -f1)" != "$sha" ]]; then
        rm -f "$archive"; tsa_error "empreinte inattendue pour $file (archive supprimée)"; exit 1
    fi
    # Quelques fichiers hors compilation C++ (liens de nix/) peuvent ne pas s'extraire : contrôle sur le dossier.
    [[ -d "$work/src/$dir" ]] || { tsa_info "Extraction $file"; tar -xzf "$archive" -C "$work/src" || true; }
    [[ -d "$work/src/$dir" ]] || { tsa_error "extraction incomplète : $work/src/$dir"; exit 1; }
done
src="$work/src/IfcOpenShell-ifcopenshell-python-0.9.0post1"

if [[ ! -f "$prefix/lib/libboost_regex.a" ]]; then
    tsa_info "Boost 1.92.0 (program_options, regex)"
    ( cd "$work/src/boost-1.92.0"
      ./bootstrap.sh --with-libraries=program_options,regex > "$work/boost-bootstrap.log" 2>&1
      ./b2 -j"$jobs" --with-program_options --with-regex link=static runtime-link=shared threading=multi variant=release \
           cxxflags=-fPIC --layout=system --prefix="$prefix" install > "$work/boost-b2.log" 2>&1 )
fi
if [[ ! -f "$prefix/share/eigen3/cmake/Eigen3Config.cmake" ]]; then
    tsa_info "Eigen 3.4.1 (en-têtes)"
    cmake -S "$work/src/eigen-3.4.1" -B "$work/build-eigen" -G Ninja -DBUILD_TESTING=OFF -DEIGEN_BUILD_DOC=OFF \
          -DEIGEN_BUILD_BLAS=OFF -DEIGEN_BUILD_LAPACK=OFF -DCMAKE_INSTALL_PREFIX="$prefix" > "$work/eigen-configure.log" 2>&1
    cmake --install "$work/build-eigen" > "$work/eigen-install.log" 2>&1
fi

tsa_info "IfcOpenShell 0.9.0post1 (journaux : $work/ifcopenshell-*.log)"
# OCCT par ses dossiers, comme sous Windows ; RUNPATH : modules à côté des bibliothèques, OCCT de TSA.
# -Bsymbolic : chaque bibliothèque et module garde ses propres instanciations std et Boost. Sans lui, le chargeur ELF
# les résolvait vers celles de libgmsh.so (SDK Gmsh compilé avec GCC 6, chargé avant les modules) : plantage dans
# faceset_helper (std::map) dès le premier fichier IFC (BUG-103, docs/LINUX_BUILD.md §11).
tsa_run cmake -S "$src/cmake" -B "$work/build-ifcopenshell" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DBUILD_IFCPYTHON=OFF -DBUILD_CONVERT=OFF -DBUILD_GEOMSERVER=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCUMENTATION=OFF \
    -DWITH_CGAL=OFF -DWITH_MANIFOLD=OFF -DCOLLADA_SUPPORT=OFF -DGLTF_SUPPORT=OFF -DUSD_SUPPORT=OFF -DWITH_PROJ=OFF \
    "-DSCHEMA_VERSIONS=2x3;4;4x3_add2" \
    "-DOCC_INCLUDE_DIR=$occt/include/opencascade" "-DOCC_LIBRARY_DIR=$occt/lib" \
    "-DBOOST_ROOT=$prefix" -DBoost_USE_STATIC_LIBS=ON -DBoost_NO_SYSTEM_PATHS=ON "-DEIGEN_DIR=$work/src/eigen-3.4.1" \
    "-DCMAKE_INSTALL_RPATH=\$ORIGIN;$occt/lib" \
    -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-Bsymbolic -DCMAKE_MODULE_LINKER_FLAGS=-Wl,-Bsymbolic \
    > "$work/ifcopenshell-configure.log" 2>&1
cmake --build "$work/build-ifcopenshell" --parallel "$jobs" > "$work/ifcopenshell-build.log" 2>&1
cmake --install "$work/build-ifcopenshell" > "$work/ifcopenshell-install.log" 2>&1

mkdir -p "$prefix/licenses"
cp "$src/COPYING" "$prefix/licenses/IfcOpenShell-COPYING" 2>/dev/null || true
cp "$src/COPYING.LESSER" "$prefix/licenses/IfcOpenShell-COPYING.LESSER" 2>/dev/null || true
tsa_info "IfcOpenShell installé : $prefix (configurer TSA avec -DTSA_IFCOPENSHELL_ROOT=$prefix)"
