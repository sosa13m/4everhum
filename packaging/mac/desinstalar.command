#!/bin/bash
# Desinstalador de 4EVERHUM para macOS (doble clic, o en Terminal: bash desinstalar.command).
# Quita el VST3, el AU y la app. Tus .mid y .wav guardados NO se borran.

set -euo pipefail

BASE="${FOUREVERHUM_HOME:-$HOME}"
rm -rf "$BASE/Library/Audio/Plug-Ins/VST3/4EVERHUM.vst3" \
       "$BASE/Library/Audio/Plug-Ins/Components/4EVERHUM.component" \
       "$BASE/Applications/4EVERHUM"

if [ -z "${FOUREVERHUM_HOME:-}" ]; then
    killall -9 AudioComponentRegistrar 2>/dev/null || true
fi

printf '\n  4EVERHUM desinstalado.\n\n'
