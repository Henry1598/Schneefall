#!/usr/bin/env bash
# Symbole aus symbole/schneefall.svg erzeugen (Host mit rsvg-convert und ImageMagick; Ergebnis wird eingecheckt):
#   symbole/schneefall.ico  (16/24/32/48/64/128/256 px, PNG-komprimiert)   symbole/schneefall-256.png
# Aufruf: bash tools/symbol-erzeugen.sh
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../symbole"
tmp="$(mktemp -d)"; trap 'rm -rf -- "$tmp"' EXIT
for g in 16 24 32 48 64 128 256; do rsvg-convert -w "$g" -h "$g" schneefall.svg -o "$tmp/$g.png"; done
cp "$tmp/256.png" schneefall-256.png
magick "$tmp/16.png" "$tmp/24.png" "$tmp/32.png" "$tmp/48.png" "$tmp/64.png" "$tmp/128.png" "$tmp/256.png" schneefall.ico
