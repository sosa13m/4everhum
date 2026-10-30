#!/bin/bash
# Instalador de 4EVERHUM para macOS (doble clic, o en Terminal: bash instalar.command)
#
#  - Plugin VST3  -> ~/Library/Audio/Plug-Ins/VST3       (FL Studio, Ableton, Reaper...)
#  - Plugin AU    -> ~/Library/Audio/Plug-Ins/Components (Logic, GarageBand, FL Studio...)
#  - App y hum2midi -> ~/Applications/4EVERHUM
#
# No pide contrasena de administrador: todo va en tu carpeta de usuario.
# Variables para pruebas: FOUREVERHUM_HOME cambia la carpeta base (por defecto $HOME).

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BASE="${FOUREVERHUM_HOME:-$HOME}"
VST3_DIR="$BASE/Library/Audio/Plug-Ins/VST3"
AU_DIR="$BASE/Library/Audio/Plug-Ins/Components"
APP_DIR="$BASE/Applications/4EVERHUM"

say()  { printf '  > %s\n' "$1"; }
fail() { printf '\n  Error: %s\n\n' "$1" >&2; exit 1; }

printf '\n  4EVERHUM - tararea -> MIDI\n\n'

[ -d "$HERE/4EVERHUM.vst3" ] || fail "No encontre 4EVERHUM.vst3 junto al instalador. Descomprime el zip completo y vuelve a intentar."

install_bundle() { # origen destino-dir
    local src="$1" dst_dir="$2" name
    name="$(basename "$src")"
    mkdir -p "$dst_dir"
    rm -rf "${dst_dir:?}/$name" || fail "No pude reemplazar $name. Cierra tu DAW y vuelve a intentar."
    cp -R "$src" "$dst_dir/"
    # Archivos bajados de internet: quitar la cuarentena para que macOS no los bloquee.
    xattr -dr com.apple.quarantine "$dst_dir/$name" 2>/dev/null || true
}

say "Plugin VST3 -> $VST3_DIR"
install_bundle "$HERE/4EVERHUM.vst3" "$VST3_DIR"

if [ -d "$HERE/4EVERHUM.component" ]; then
    say "Plugin AU -> $AU_DIR"
    install_bundle "$HERE/4EVERHUM.component" "$AU_DIR"
fi

say "App y hum2midi -> $APP_DIR"
mkdir -p "$APP_DIR"
[ -d "$HERE/4EVERHUM.app" ] && install_bundle "$HERE/4EVERHUM.app" "$APP_DIR"
if [ -f "$HERE/hum2midi" ]; then
    cp "$HERE/hum2midi" "$APP_DIR/hum2midi"
    chmod +x "$APP_DIR/hum2midi"
    xattr -d com.apple.quarantine "$APP_DIR/hum2midi" 2>/dev/null || true
fi
for doc in LEEME-mac.txt desinstalar.command LICENSE TERCEROS.md; do
    [ -f "$HERE/$doc" ] && cp "$HERE/$doc" "$APP_DIR/"
done
chmod +x "$APP_DIR/desinstalar.command" 2>/dev/null || true

# Que macOS vuelva a escanear los Audio Units.
if [ -z "${FOUREVERHUM_HOME:-}" ]; then
    killall -9 AudioComponentRegistrar 2>/dev/null || true
fi

printf '\n  Listo.\n'
printf '  FL Studio: Options > Manage plugins > Find plugins, y busca 4EVERHUM.\n'
printf '  Logic: aparece en Audio Units > 4EVERTOOLS > 4EVERHUM.\n'
printf '  La app sin DAW esta en %s\n\n' "$APP_DIR/4EVERHUM.app"
