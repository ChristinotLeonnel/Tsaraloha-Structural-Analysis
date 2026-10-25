#!/usr/bin/env bash
# Configure TSA sous Linux (preset linux-debug ou linux-release).
# Usage : scripts/linux/configure.sh [debug|release] [options CMake supplémentaires...]
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

mode="${1:-debug}"; shift || true
tsa_build_type "$mode" >/dev/null

bash "$TSA_SOURCE_DIR/scripts/linux/check-dependencies.sh" || { tsa_error "dépendances obligatoires manquantes (voir ci-dessus)"; exit 1; }

qt="$(tsa_find_qt)"; occt="$(tsa_find_occt)"
export CMAKE_PREFIX_PATH="$occt:$qt${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"   # séparateur « : » sous Unix
# Dépendances facultatives trouvées : placées avant "$@", une option explicite l'emporte.
opt_args=()
gmsh="$(tsa_find_gmsh)"
[[ -n "$gmsh" ]] && opt_args+=("-DTSA_GMSH_SDK=$gmsh")
ifc="$(tsa_find_ifcopenshell)"
[[ -n "$ifc" ]] && opt_args+=("-DTSA_IFCOPENSHELL_ROOT=$ifc")
cd "$TSA_SOURCE_DIR"
tsa_run cmake --preset "linux-${mode,,}" "${opt_args[@]}" "$@"
