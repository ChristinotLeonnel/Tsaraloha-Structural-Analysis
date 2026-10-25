# Fonctions communes des scripts Linux de TSA (docs/LINUX_BUILD.md). À sourcer, pas à exécuter.
#
# Variables reconnues (toutes facultatives) :
#   TSA_QT_DIR    préfixe Qt 6 (contient lib/cmake/Qt6), ex. ~/Qt/6.12.0/gcc_64
#   TSA_OCCT_DIR  préfixe OpenCASCADE 8 (contient lib/cmake/opencascade), ex. ~/opt/occt-8.0.1
#   TSA_GMSH_SDK  SDK Gmsh Linux (contient include/gmsh.h_cwrap), ex. ~/opt/gmsh-4.15.2-Linux64-sdk
#   TSA_IFCOPENSHELL_ROOT  IfcOpenShell (scripts/linux/build-ifcopenshell.sh), ex. ~/opt/ifcopenshell-0.9.0post1
#   TSA_JOBS      nombre de compilations parallèles (défaut : nproc)

TSA_SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

tsa_info()  { printf '\033[1;34m[TSA]\033[0m %s\n' "$*"; }
tsa_error() { printf '\033[1;31m[TSA] ERREUR :\033[0m %s\n' "$*" >&2; }
tsa_run()   { printf '\033[1;32m$\033[0m'; printf ' %q' "$@"; printf '\n'; "$@"; }

# debug|release -> Debug|Release (ou erreur)
tsa_build_type() {
    case "${1:-debug}" in
        debug|Debug)     echo Debug ;;
        release|Release) echo Release ;;
        *) tsa_error "mode inconnu « $1 » (attendu : debug ou release)"; return 1 ;;
    esac
}

# Préfixe Qt 6 : TSA_QT_DIR, sinon la version la plus récente de ~/Qt/6.*/gcc_64, sinon Qt du système.
tsa_find_qt() {
    if [[ -n "${TSA_QT_DIR:-}" ]]; then echo "$TSA_QT_DIR"; return; fi
    local d
    d="$(ls -d "$HOME"/Qt/6.*/gcc_64 2>/dev/null | sort -V | tail -n 1 || true)"
    if [[ -n "$d" ]]; then echo "$d"; return; fi
    if [[ -f /usr/lib/x86_64-linux-gnu/cmake/Qt6/Qt6Config.cmake ]]; then echo /usr; fi
}

# Préfixe OCCT 8 : TSA_OCCT_DIR, sinon la version la plus récente de ~/opt/occt-8.*, sinon /usr/local.
tsa_find_occt() {
    if [[ -n "${TSA_OCCT_DIR:-}" ]]; then echo "$TSA_OCCT_DIR"; return; fi
    local d
    d="$(ls -d "$HOME"/opt/occt-8.* 2>/dev/null | sort -V | tail -n 1 || true)"
    if [[ -n "$d" ]]; then echo "$d"; return; fi
    if [[ -f /usr/local/lib/cmake/opencascade/OpenCASCADEConfig.cmake ]]; then echo /usr/local; fi
}

# SDK Gmsh (facultatif, maillage) : TSA_GMSH_SDK, sinon la version la plus récente de ~/opt/gmsh-*-Linux64-sdk.
tsa_find_gmsh() {
    if [[ -n "${TSA_GMSH_SDK:-}" ]]; then echo "$TSA_GMSH_SDK"; return; fi
    ls -d "$HOME"/opt/gmsh-*-Linux64-sdk 2>/dev/null | sort -V | tail -n 1 || true
}

# IfcOpenShell (facultatif, import IFC complet) : TSA_IFCOPENSHELL_ROOT, sinon ~/opt/ifcopenshell-* le plus récent.
tsa_find_ifcopenshell() {
    if [[ -n "${TSA_IFCOPENSHELL_ROOT:-}" ]]; then echo "$TSA_IFCOPENSHELL_ROOT"; return; fi
    ls -d "$HOME"/opt/ifcopenshell-* 2>/dev/null | sort -V | tail -n 1 || true
}
