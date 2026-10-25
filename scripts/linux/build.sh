#!/usr/bin/env bash
# Compile TSA sous Linux (configure d'abord si le dossier de build n'existe pas).
# Usage : scripts/linux/build.sh [debug|release] [cibles...]     ex. build.sh release TSA
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

mode="${1:-debug}"; shift || true
tsa_build_type "$mode" >/dev/null
mode="${mode,,}"

if [[ ! -f "$TSA_SOURCE_DIR/build-linux-$mode/CMakeCache.txt" ]]; then
    bash "$TSA_SOURCE_DIR/scripts/linux/configure.sh" "$mode"
fi
cd "$TSA_SOURCE_DIR"
targets=()
for t in "$@"; do targets+=(--target "$t"); done
tsa_run cmake --build --preset "linux-$mode" "${targets[@]}" -- -j "${TSA_JOBS:-$(nproc)}"
