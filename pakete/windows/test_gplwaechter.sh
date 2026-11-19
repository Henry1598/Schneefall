#!/usr/bin/env bash
# Test des GPL-Wächters (pakete/windows/gplwaechter.sh) mit Beispielangaben; reines Bash, läuft auf Linux und MSYS2.
cd "$(dirname "${BASH_SOURCE[0]}")" && source ./gplwaechter.sh
fehler=0
# erwartet: lizenz_ok "<Angabe>" -> ok|abbruch
l() { local erw="$1" lic="$2" ist=abbruch; lizenz_ok "$lic" && ist=ok
      [ "$ist" = "$erw" ] && echo "PASS: Lizenz '$lic' -> $ist" || { echo "FAIL: Lizenz '$lic' -> $ist, erwartet $erw"; fehler=1; }; }
d() { local erw="$1" datei="$2" ist=frei; datei_gesperrt "$datei" && ist=gesperrt
      [ "$ist" = "$erw" ] && echo "PASS: Datei '$datei' -> $ist" || { echo "FAIL: Datei '$datei' -> $ist, erwartet $erw"; fehler=1; }; }
l abbruch "GPL-3.0-or-later"
l ok      "LGPL-2.1-or-later"
l abbruch "GPL-3.0-or-later  LGPL-2.1-or-later"
l ok      "GPL-2.0-or-later OR FTL"
l ok      "GPL-3.0-or-later WITH GCC-exception-3.1"
l abbruch "None"
l abbruch ""
l ok      "spdx:MIT  spdx:BSD-3-Clause"
l abbruch "LGPL-2.1-or-later AND GPL-2.0-only"
l abbruch "AGPL-3.0-only"
l ok      "(LGPL-3.0-only OR GPL-2.0-or-later) AND MIT"
l abbruch "MIT  None"
l ok      "LGPL-2.1-or-later  documentation:GPL-3.0-or-later"
l abbruch "documentation:GPL-3.0-or-later  GPL-2.0-only"
l abbruch "GPL-3.0-or-later AND LGPL-2.1-or-later"
GPL_AUSNAHMEN="$(mktemp)"; printf 'qt6-base  # Test\ngettext-runtime\n' > "$GPL_AUSNAHMEN"
for x in "qt6-base:ja" "gettext-runtime:ja" "qt6-bas:nein" "ffmpeg:nein"; do
    lizenz_ausnahme "${x%%:*}" && ist=ja || ist=nein
    [ "$ist" = "${x##*:}" ] && echo "PASS: Ausnahme ${x%%:*} -> $ist" || { echo "FAIL: Ausnahme ${x%%:*} -> $ist"; fehler=1; }
done
rm -f "$GPL_AUSNAHMEN"; unset GPL_AUSNAHMEN
for n in avcodec-61.dll libavformat-61.dll AVUTIL-59.dll swscale-8.dll swresample-5.dll postproc-58.dll libx264-164.dll \
         libx265-215.dll ffmpegmediaplugin.dll multimedia/ffmpegmediaplugin.dll libxvidcore-4.dll libmp3lame-0.dll libgnutls-30.dll; do d gesperrt "$n"; done
for n in Qt6Core.dll Qt6Multimedia.dll libstdc++-6.dll platforms/qwindows.dll libavif.dll; do d frei "$n"; done
exit $fehler
