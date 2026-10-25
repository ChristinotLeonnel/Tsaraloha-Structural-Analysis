#!/usr/bin/env bash
# Lance la suite de tests TSA_TestSuite.
# Usage : scripts/linux/test.sh [debug|release] [--suite=<nom>]
#
# Plateforme Qt : QT_QPA_PLATFORM s'il est défini ; sinon « xcb » dans une session graphique où le plugin X11 de Qt
# est utilisable (vues OCCT réelles, comme « windows » sous Windows), sinon « offscreen » : les tests de vue 3D
# protégés sont alors [SKIP], et ceux qui ne le sont pas arrêtent le programme (BadWindow).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

mode="${1:-debug}"; shift || true
tsa_build_type "$mode" >/dev/null
mode="${mode,,}"

exe="$TSA_SOURCE_DIR/build-linux-$mode/TSA_TestSuite"
[[ -x "$exe" ]] || { tsa_error "$exe absent : lancer d'abord scripts/linux/build.sh $mode"; exit 1; }

if [[ -z "${QT_QPA_PLATFORM:-}" ]]; then
    xcb="$(tsa_find_qt)/plugins/platforms/libqxcb.so"
    if [[ -n "${DISPLAY:-}" && -f "$xcb" ]] && ! ldd "$xcb" | grep -q "not found"; then
        export QT_QPA_PLATFORM=xcb
    else
        export QT_QPA_PLATFORM=offscreen
        tsa_info "plateforme Qt « offscreen » (pas d'affichage X11 utilisable) : tests de vue 3D limités"
    fi
fi
tsa_info "QT_QPA_PLATFORM=$QT_QPA_PLATFORM"
# Les tests écrivent leurs fichiers dans ./build/test_tsa_data, relatif au dossier courant.
cd "$TSA_SOURCE_DIR/build-linux-$mode"
tsa_run "$exe" "$@"
