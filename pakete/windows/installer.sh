#!/usr/bin/env bash
# Setup.exe mit Inno Setup 6 bauen und Prüfsummen schreiben (nach paket.sh; MSYS2 oder Git-Bash unter Windows).
# Ergebnis: dist/Schneefall-<version>-windows-Setup.exe und dist/SHA256SUMS (Setup + ZIP)
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
VERSION="$(tr -d '\r\n ' < VERSION)"
DIST="${DIST:-dist}"
[ -x "$DIST/Schneefall/schneefall.exe" ] || { echo "installer.sh: erst paket.sh ausführen" >&2; exit 1; }
ISCC="${ISCC:-}"
if [ -z "$ISCC" ]; then
    for k in "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" "/c/Program Files/Inno Setup 6/ISCC.exe" \
             "$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe"; do
        [ -x "$k" ] && ISCC="$k" && break
    done
fi
[ -n "$ISCC" ] || { echo "installer.sh: ISCC.exe nicht gefunden (Inno Setup 6 installieren oder ISCC=... setzen)" >&2; exit 1; }
# Mindestversion: Inno Setup 6.7
# (ISCC.exe selbst meldet keine Version; die des Installationsordners steht in unins000.exe)
isver="$(powershell.exe -NoProfile -Command "(Get-Item -LiteralPath '$(cygpath -w "$(dirname "$ISCC")/unins000.exe")').VersionInfo.ProductVersion" 2>/dev/null | tr -d '\r ' || true)"
echo "Inno Setup: ${isver:-unbekannt}"
[ -n "$isver" ] && [ "$(printf '%s\n6.7\n' "$isver" | sort -V | head -1)" = "6.7" ] || { echo "installer.sh: Inno Setup 6.7 oder neuer nötig (gefunden: ${isver:-unbekannt})" >&2; exit 1; }
# Pfade relativ zur .iss-Datei: das Skript liegt in pakete/windows, dist/ zwei Ebenen höher
MSYS2_ARG_CONV_EXCL='*' "$ISCC" "/DVersion=$VERSION" "/Qp" "$(cygpath -w pakete/windows/schneefall.iss)"
cd "$DIST"
sha256sum "Schneefall-$VERSION-windows-Setup.exe" "Schneefall-$VERSION-windows.zip" > SHA256SUMS
cat SHA256SUMS
