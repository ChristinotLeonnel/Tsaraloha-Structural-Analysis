#!/usr/bin/env bash
# Vérifie les outils et bibliothèques nécessaires pour compiler et lancer TSA sous Linux (lecture seule).
# Code de sortie : 0 si tout ce qui est obligatoire est présent, 1 sinon.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

missing=0
ok()   { printf '  \033[32mOK\033[0m        %s\n' "$*"; }
warn() { printf '  \033[33mFACULTATIF\033[0m %s\n' "$*"; }
bad()  { printf '  \033[31mMANQUANT\033[0m  %s\n' "$*"; missing=1; }

tsa_info "Outils"
for tool in cmake ninja g++ git python3; do
    if command -v "$tool" >/dev/null; then ok "$tool : $("$tool" --version 2>&1 | head -n 1)"; else bad "$tool"; fi
done
if command -v cmake >/dev/null; then
    v="$(cmake --version | head -n 1 | grep -oE '[0-9]+\.[0-9]+' | head -n 1)"
    if [[ "$(printf '%s\n3.21\n' "$v" | sort -V | head -n 1)" != "3.21" ]]; then bad "CMake >= 3.21 requis (3DDesplacement), trouvé $v"; fi
fi

tsa_info "Qt 6 (Core Gui Widgets Svg Network ; Test facultatif)"
qt="$(tsa_find_qt)"
if [[ -n "$qt" && -f "$qt/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    ok "Qt : $qt"
    for m in Core Gui Widgets Svg Network; do
        [[ -d "$qt/lib/cmake/Qt6$m" ]] && ok "Qt6$m" || bad "Qt6$m"
    done
    [[ -d "$qt/lib/cmake/Qt6Test" ]] && ok "Qt6Test" || warn "Qt6Test (suite « shortcuts »)"
    if [[ -f "$qt/plugins/platforms/libqxcb.so" ]] && ldd "$qt/plugins/platforms/libqxcb.so" | grep -q "not found"; then
        warn "plugin xcb : $(ldd "$qt/plugins/platforms/libqxcb.so" | awk '/not found/ {print $1}' | sort -u | tr '\n' ' ')— interface X11 impossible (sudo apt install libxcb-cursor0)"
    fi
else
    bad "Qt 6 introuvable (définir TSA_QT_DIR=<préfixe contenant lib/cmake/Qt6>)"
fi

tsa_info "OpenCASCADE 8"
occt="$(tsa_find_occt)"
if [[ -n "$occt" && -f "$occt/lib/cmake/opencascade/OpenCASCADEConfig.cmake" ]]; then
    ver="$(grep -oE 'OpenCASCADE_(MAJOR|MINOR|MAINTENANCE)_VERSION +"[0-9]+"' "$occt/lib/cmake/opencascade/OpenCASCADEConfig.cmake" | grep -oE '[0-9]+' | paste -sd.)"
    ok "OCCT $ver : $occt"
    [[ "$ver" == 8.* ]] || bad "OCCT 8.x requis (code écrit pour 8.0.1)"
else
    bad "OCCT 8 introuvable (définir TSA_OCCT_DIR ; compilation : docs/LINUX_BUILD.md)"
fi

tsa_info "Bibliothèques système (en-têtes de développement)"
for pc in freetype2 gl x11; do
    pkg-config --exists "$pc" 2>/dev/null && ok "$pc" || bad "$pc (paquets libfreetype-dev libgl-dev libx11-dev)"
done

tsa_info "Dépôts voisins"
root="$(dirname "$TSA_SOURCE_DIR")"
[[ -f "$root/TSALab/science/CMakeLists.txt" ]] && ok "TSALab/science" || bad "../TSALab (cœur scientifique obligatoire)"
[[ -f "$root/3DDesplacement/CMakeLists.txt" ]] && ok "3DDesplacement (moteur fem3d)" || warn "../3DDesplacement (moteur fem3d facultatif)"
[[ -f "$TSA_SOURCE_DIR/thirdparty/eigen-3.4.1/include/eigen3/Eigen/Core" ]] && ok "Eigen (thirdparty)" || warn "Eigen thirdparty/eigen-3.4.1 (requis par fem3d)"

tsa_info "Maillage"
gmsh="$(tsa_find_gmsh)"
if [[ -n "$gmsh" && -f "$gmsh/include/gmsh.h_cwrap" ]] && ls "$gmsh"/lib/libgmsh.so* >/dev/null 2>&1; then
    ok "SDK Gmsh : $gmsh"
else
    warn "SDK Gmsh absent (TSA_GMSH_SDK ou ~/opt/gmsh-*-Linux64-sdk) : maillage et suites mesh/surfaces indisponibles"
fi

tsa_info "Import IFC"
ifc="$(tsa_find_ifcopenshell)"
if [[ -n "$ifc" && -f "$ifc/lib/cmake/IfcOpenShell/IfcOpenShellConfig.cmake" ]]; then
    ok "IfcOpenShell : $ifc"
else
    warn "IfcOpenShell absent (scripts/linux/build-ifcopenshell.sh) : import IFC par le lecteur STEP interne, test 400 en échec"
fi

tsa_info "Exécution"
command -v OpenSees >/dev/null && ok "OpenSees : $(command -v OpenSees)" || warn "OpenSees absent du PATH (moteur OpenSees et suites opensees/extraction)"

exit "$missing"
