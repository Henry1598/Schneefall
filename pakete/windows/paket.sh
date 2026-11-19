#!/usr/bin/env bash
# Windows-Paket bauen (MSYS2 UCRT64): Release-Build, windeployqt, benötigte ucrt64-DLLs (per ntldd ermittelt),
# Mesa-Software-OpenGL nur als Rückfall (opengl32sw.dll). Ergebnis in dist/:
#   dist/Schneefall/                          – lauffähiger Ordner
#   dist/Schneefall-<version>-windows.zip     – derselbe Ordner als ZIP
# Aufruf: make PLATTFORM=windows paket   (oder: bash pakete/windows/paket.sh)
# Voraussetzung: gcc, make, pkgconf, qt6-base/-multimedia/-svg/-tools, ntldd, zip, mesa (alles mingw-w64-ucrt-x86_64-*)
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
source pakete/windows/gplwaechter.sh   # datei_gesperrt, lizenz_ok (Test: pakete/windows/test_gplwaechter.sh)

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
# Qt Multimedia: Schneefall nutzt nur QAudioSink (eigene Klänge, src/klang.cpp); das spricht Windows-Audio (WASAPI) direkt aus
# Qt6Multimedia.dll an. Das Medien-Plugin multimedia/ffmpegmediaplugin.dll (FFmpeg, im MSYS2 mit --enable-gpl gebaut, zieht x264,
# x265, gnutls … nach) wird nur zum Abspielen/Kodieren von Medien gebraucht und bleibt weg: sonst wäre das Paket GPL-belastet.
windeployqt6 --release --no-translations --no-opengl-sw --no-system-d3d-compiler --no-compiler-runtime \
    --skip-plugin-types multimedia --dir "$ORDNER" "$ORDNER/schneefall.exe"
rm -rf -- "$ORDNER/multimedia"
# Symbol-Plugins ausdrücklich sicherstellen (Icon-Themes/Bilder können SVG sein)
PLUGINS="$PREFIX/share/qt6/plugins"
for p in iconengines/qsvgicon.dll imageformats/qsvg.dll; do
    [ -f "$ORDNER/$p" ] || { [ -f "$PLUGINS/$p" ] && mkdir -p "$ORDNER/$(dirname "$p")" && cp "$PLUGINS/$p" "$ORDNER/$p"; } || echo "Hinweis: $p nicht gefunden"
done

# MSYS2-Qt ist verschiebbar: ohne qt.conf sucht es Plugins in <Programmordner>\..\share\qt6\plugins (bei einer ZIP unter
# C:\Schneefall also in C:\share\qt6\plugins, für jeden Benutzer anlegbar = Plugin-Hijacking). qt.conf bindet Qt an den Ordner.
printf '[Paths]\r\nPrefix = .\r\nPlugins = .\r\n' > "$ORDNER/qt.conf"

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
# Qt darf Plugins und Bibliotheken nur im Programmordner suchen (siehe qt.conf)
pfade="$(PATH=/c/Windows/System32:/c/Windows "$ORDNER/schneefall.exe" --diagnose-pfade | tr -d '\r')"
echo "$pfade"
prog="$(echo "$pfade" | sed -n 's/^programmordner=//p')"
[ -n "$prog" ] || fehler "--diagnose-pfade liefert keinen Programmordner"
fremd="$(echo "$pfade" | sed -n 's/^\(bibliothekspfad\|pluginpfad\)=//p' | grep -v -x -F "$prog" || true)"
[ -z "$fremd" ] || fehler $'Qt sucht Plugins außerhalb des Programmordners:\n'"$fremd"
# Dateieigenschaften (VERSIONINFO aus schneefall.rc): Produktversion und Firma müssen stimmen
eig="$(powershell.exe -NoProfile -Command "\$v=(Get-Item -LiteralPath '$(cygpath -w "$ORDNER/schneefall.exe")').VersionInfo; \$v.ProductVersion+'|'+\$v.CompanyName+'|'+\$v.FileDescription" | tr -d '\r')"
[[ "$eig" == "$VERSION|Henry1598|"* ]] || fehler "Dateieigenschaften falsch: '$eig'"
# Lizenztexte: je gepackter Datei das MSYS2-Paket ermitteln (pacman -Qo), dessen Lizenzordner nach lizenzen/<paket>/ kopieren
# und DRITTANBIETER.txt (Datei -> Paket -> Version -> Lizenz) schreiben
echo "== Lizenzen"
mkdir -p "$ORDNER/lizenzen"
TABELLE="$(mktemp)"; PAKETE="$(mktemp)"; trap 'rm -f -- "$TABELLE" "$PAKETE"' EXIT
fehlende_lizenzen=""
gpl_dateien=""
while IFS= read -r datei; do
    rel="${datei#"$ORDNER"/}"
    [ "$rel" = "schneefall.exe" ] && continue
    datei_gesperrt "$rel" && gpl_dateien+="$rel (Dateiname auf der Sperrliste)"$'\n'
    quelle=""
    for k in "$PREFIX/bin/$rel" "$PREFIX/share/qt6/plugins/$rel"; do [ -f "$k" ] && quelle="$k" && break; done
    [ "$rel" = "opengl32sw.dll" ] && quelle="$PREFIX/bin/opengl32.dll"
    pak=""
    [ -n "$quelle" ] && pak="$(pacman -Qoq "$quelle" 2>/dev/null || true)"
    if [ -z "$pak" ]; then
        printf '%s\t%s\t%s\t%s\n' "$rel" "(unbekannt)" "-" "-" >> "$TABELLE"; fehlende_lizenzen+="$rel "$'\n'
        continue
    fi
    echo "$pak" >> "$PAKETE"
    ver="$(pacman -Q "$pak" | awk '{print $2}')"
    lic="$(pacman -Qi "$pak" | sed -n 's/^Licenses *: *//p' | sed 's/spdx://g')"
    # Starkes Copyleft verboten (gplwaechter.sh): Sperrliste für Dateinamen (FFmpeg & Co.); jeder Lizenzeintrag des Pakets einzeln
    # bewertet (mehrere Einträge gelten alle); GPL nur mit LGPL-Alternative oder Ausnahme (WITH …) ohne weiteren reinen GPL-Teil;
    # Lizenz "None"/leer ist ein Abbruch
    datei_gesperrt "$rel" && gpl_dateien+="$rel -> $pak (Dateiname auf der Sperrliste)"$'\n'
    lizenz_ok "$lic" || lizenz_ausnahme "${pak#mingw-w64-ucrt-x86_64-}" && [ -n "$lic" ] && [ "$lic" != "None" ] || gpl_dateien+="$rel -> $pak (Lizenz: '${lic:-leer}')"$'\n'
    printf '%s\t%s\t%s\t%s\n' "$rel" "$pak" "$ver" "$lic" >> "$TABELLE"
done < <(find "$ORDNER" -type f \( -iname '*.dll' -o -iname '*.exe' \) | LC_ALL=C sort)
while IFS= read -r pak; do
    kurz="${pak#mingw-w64-ucrt-x86_64-}"
    # Lizenzordner laut Paketliste (Namen weichen vom Paketnamen ab, z. B. glib2 für glib2)
    mapfile -t ordner < <(pacman -Qlq "$pak" | grep -E "^$PREFIX/share/licenses/[^/]+/?$" | sed 's:/$::' || true)
    if [ "${#ordner[@]}" = 0 ]; then
        # Manche MSYS2-Pakete (z. B. ffmpeg, x265, gnutls) bringen keinen Lizenztext mit. Dann Lizenzangabe und Projektseite aus
        # den Paketdaten festhalten; die Standardtexte (GPL, LGPL, Apache, MIT …) liegen in lizenzen/qt6-base. Ohne Lizenzangabe: Abbruch.
        lic="$(pacman -Qi "$pak" | sed -n 's/^Licenses *: *//p' | sed 's/spdx://g')"
        if [ -z "$lic" ] || [ "$lic" = "None" ]; then
            fehlende_lizenzen+="$pak: weder Lizenzordner noch Lizenzangabe im MSYS2-Paket"$'\n'
            continue
        fi
        mkdir -p "$ORDNER/lizenzen/$kurz"
        {
            echo "Paket:   $pak $(pacman -Q "$pak" | awk '{print $2}')"
            echo "Lizenz:  $lic"
            echo "Projekt: $(pacman -Qi "$pak" | sed -n 's/^URL *: *//p')"
            echo
            echo "Das MSYS2-Paket enthält keinen eigenen Lizenztext. Die Standardtexte der genannten Lizenzen stehen in"
            echo "lizenzen/qt6-base (GPL-2.0, GPL-3.0, LGPL-2.1, LGPL-3.0, Apache-2.0, MIT, BSD usw.)."
        } > "$ORDNER/lizenzen/$kurz/LIZENZ.txt"
        continue
    fi
    for o in "${ordner[@]}"; do mkdir -p "$ORDNER/lizenzen/$(basename "$o")" && cp -r "$o"/. "$ORDNER/lizenzen/$(basename "$o")/"; done
done < <(sort -u "$PAKETE")
{
    cat pakete/windows/DRITTANBIETER-kopf.txt
    echo "Datei -> MSYS2-Paket -> Version -> Lizenz"
    echo "-----------------------------------------"
    awk -F'\t' '{printf "%s -> %s -> %s -> %s\n", $1, $2, $3, $4}' "$TABELLE"
    echo
    echo "Quellcode der MSYS2-Pakete (genaue Versionen dieses Pakets)"
    echo "-----------------------------------------------------------"
    while IFS= read -r pak; do
        # Quellpaket = %BASE% aus der pacman-Datenbank (z. B. libstdc++ -> mingw-w64-gcc, libwinpthread -> mingw-w64-winpthreads,
        # llvm-libs -> mingw-w64-llvm), sonst der Paketname ohne "ucrt-x86_64"
        ver="$(pacman -Q "$pak" | awk '{print $2}')"
        basis="$(sed -n '/^%BASE%$/{n;p}' "/var/lib/pacman/local/$pak-$ver/desc" 2>/dev/null || true)"
        [ -n "$basis" ] || basis="${pak/-ucrt-x86_64-/-}"
        printf 'https://repo.msys2.org/mingw/sources/%s-%s.src.tar.zst\n' "$basis" "$ver"
    done < <(sort -u "$PAKETE")
    echo
    echo "(Paketversionen und Lizenzangaben laut MSYS2 zum Zeitpunkt des Baus.)"
} > "$ORDNER/DRITTANBIETER.txt"
[ -z "$fehlende_lizenzen" ] || echo "Hinweis: ohne Paket oder Lizenztext:"$'\n'"$fehlende_lizenzen" >&2
[ -z "$gpl_dateien" ] || fehler $'GPL-Bibliotheken im Paket (nicht erlaubt):\n'"$gpl_dateien"
# Pflicht: jede gepackte Datei muss einem Paket mit Lizenztext zugeordnet sein
[ -z "$fehlende_lizenzen" ] || fehler "Lizenzzuordnung unvollständig (siehe oben)"
cp VERSION "$ORDNER/VERSION.txt"
# Paketliste der Bauumgebung (alle MSYS2-Pakete mit Version) fürs Release
pacman -Q > "$DIST/pakete.txt"
( cd "$DIST" && zip -qr -9 "$ZIP" Schneefall )
echo "Fertig: $ORDNER ($(du -sh "$ORDNER" | cut -f1)), $DIST/$ZIP ($(du -h "$DIST/$ZIP" | cut -f1))"
