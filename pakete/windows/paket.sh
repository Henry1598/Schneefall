#!/usr/bin/env bash
# Windows-Paket bauen (MSYS2 UCRT64): Release-Build, windeployqt, benötigte ucrt64-DLLs (per ntldd ermittelt),
# Mesa-Software-OpenGL nur als Rückfall (opengl32sw.dll). Ergebnis in dist/:
#   dist/Schneefall/                          – lauffähiger Ordner
#   dist/Schneefall-<version>-windows.zip     – derselbe Ordner als ZIP
# Aufruf: make PLATTFORM=windows paket   (oder: bash pakete/windows/paket.sh)
# Voraussetzung: gcc, make, pkgconf, qt6-base/-multimedia/-svg/-tools, ntldd, zip, mesa (alles mingw-w64-ucrt-x86_64-*)
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

VERSION="$(tr -d '\r\n ' < VERSION)"
B="${B:-build-paket}"
DIST="${DIST:-dist}"
ORDNER="$DIST/Schneefall"
ZIP="Schneefall-$VERSION-windows.zip"
PREFIX="$(cygpath -u "$(pkg-config --variable=prefix Qt6Core)")"   # z. B. /ucrt64
MAKE="$(command -v mingw32-make || command -v make)"

fehler() { echo "paket.sh: $*" >&2; exit 1; }
for w in ntldd zip windeployqt6 cygpath strip; do command -v "$w" >/dev/null || fehler "Werkzeug fehlt: $w"; done
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+([-.][0-9A-Za-z.]+)?$ ]] || fehler "ungültige Version in VERSION: $VERSION"

echo "== 1/6 Release-Build $VERSION"
CXXFLAGS="-O2 -DNDEBUG -Wall -Wextra" "$MAKE" PLATTFORM=windows B="$B" -j"$(nproc)" "$B/schneefall.exe"

echo "== 2/6 Ordner anlegen"
[ -n "$DIST" ] && [ "$DIST" != "/" ] && [ "$DIST" != "." ] || fehler "ungültiges DIST: $DIST"
rm -rf -- "$ORDNER" "$DIST/$ZIP"
mkdir -p "$ORDNER"
cp "$B/schneefall.exe" "$ORDNER/"
strip --strip-unneeded "$ORDNER/schneefall.exe"

echo "== 3/6 windeployqt (Qt-DLLs und Plugins)"
# Schneefall hat eigene Übersetzungen nicht nötig; OpenGL-Software und Compiler-Laufzeit kommen unten per ntldd
windeployqt6 --release --no-translations --no-opengl-sw --no-system-d3d-compiler --no-compiler-runtime \
    --dir "$ORDNER" "$ORDNER/schneefall.exe"
# Symbol-Plugins ausdrücklich sicherstellen (Icon-Themes/Bilder können SVG sein)
PLUGINS="$PREFIX/share/qt6/plugins"
for p in iconengines/qsvgicon.dll imageformats/qsvg.dll; do
    [ -f "$ORDNER/$p" ] || { [ -f "$PLUGINS/$p" ] && mkdir -p "$ORDNER/$(dirname "$p")" && cp "$PLUGINS/$p" "$ORDNER/$p"; } || echo "Hinweis: $p nicht gefunden"
done

echo "== 4/6 Mesa als Rückfall (opengl32sw.dll)"
# Qt nimmt opengl32sw.dll nur, wenn kein brauchbarer OpenGL-Treiber da ist; auf echter GPU bleibt das native opengl32.dll.
# Deshalb darf NIE eine opengl32.dll in den Ordner (sie würde den Grafiktreiber verdecken).
[ -f "$PREFIX/bin/opengl32.dll" ] || fehler "Mesa (mingw-w64-ucrt-x86_64-mesa) fehlt: $PREFIX/bin/opengl32.dll"
cp "$PREFIX/bin/opengl32.dll" "$ORDNER/opengl32sw.dll"

echo "== 5/6 ucrt64-DLLs per ntldd (Runden, bis nichts Neues kommt)"
# Alle Abhängigkeiten aller exe/dll im Ordner, die unter $PREFIX liegen, nach $ORDNER kopieren, bis nichts Neues kommt.
# Systemeigene DLLs (C:\Windows) und opengl32.dll (siehe oben) werden nie mitgegeben.
abhaengigkeiten() { # Datei -> Zeilen "name<TAB>pfad" (pfad leer = nicht gefunden)
    ntldd "$1" 2>/dev/null | awk '
        /=>/ { n=$1; sub(/^[ \t]+/,"",n); p=$0; sub(/^.*=> */,"",p); sub(/ *\(0x[0-9a-fA-F]+\).*$/,"",p);
               if (p ~ /not found/) p=""; print n "\t" p }'
}
for runde in $(seq 1 12); do
    neu=0
    while IFS= read -r datei; do
        while IFS=$'\t' read -r name pfad; do
            [ -n "$name" ] || continue
            lname="${name,,}"
            [ "$lname" = "opengl32.dll" ] && continue
            [ -f "$ORDNER/$name" ] && continue
            if [ -z "$pfad" ]; then
                # API-Sets (api-ms-win-*, ext-ms-*) kennt ntldd nicht immer; nur echte fehlende DLLs sind ein Fehler (unten geprüft)
                continue
            fi
            upfad="$(cygpath -u "$pfad")"
            case "$upfad" in
                "$PREFIX"/*) cp "$upfad" "$ORDNER/$name"; neu=1; echo "  + $name";;
            esac
        done < <(abhaengigkeiten "$datei")
    done < <(find "$ORDNER" -type f \( -iname '*.exe' -o -iname '*.dll' \))
    [ "$neu" = 0 ] && break
done

echo "== 6/6 Prüfen und packen"
# a) keine opengl32.dll im Ordner, b) nichts Fehlendes außer API-Sets, c) Programm startet
find "$ORDNER" -iname 'opengl32.dll' | grep -q . && fehler "opengl32.dll im Paket (würde den Grafiktreiber verdecken)"
fehlend=""
while IFS= read -r datei; do
    f="$(ntldd "$datei" 2>/dev/null | grep -i 'not found' | grep -vi -E 'api-ms-|ext-ms-' | awk '{print $1}' || true)"
    [ -n "$f" ] && fehlend+="$(basename "$datei"): $f"$'\n'
done < <(find "$ORDNER" -type f \( -iname '*.exe' -o -iname '*.dll' \))
[ -z "$fehlend" ] || fehler $'fehlende DLLs im Paket:\n'"$fehlend"
gemeldet="$(PATH=/c/Windows/System32:/c/Windows "$ORDNER/schneefall.exe" --version | tr -d '\r')"
[ "$gemeldet" = "schneefall $VERSION" ] || fehler "--version meldet '$gemeldet', erwartet 'schneefall $VERSION'"
# Lizenztexte (Qt = LGPL, Mesa, ffmpeg …) best effort beilegen
mkdir -p "$ORDNER/lizenzen"
for d in "$PREFIX"/share/licenses/*qt6* "$PREFIX"/share/licenses/*mesa* "$PREFIX"/share/licenses/*ffmpeg*; do
    [ -d "$d" ] && cp -r "$d" "$ORDNER/lizenzen/" || true
done
cp VERSION "$ORDNER/VERSION.txt"
( cd "$DIST" && zip -qr -9 "$ZIP" Schneefall )
echo "Fertig: $ORDNER ($(du -sh "$ORDNER" | cut -f1)), $DIST/$ZIP ($(du -h "$DIST/$ZIP" | cut -f1))"
