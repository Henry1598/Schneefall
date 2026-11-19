#!/usr/bin/env bash
# GPL-Wächter für das Windows-Paket (von paket.sh eingebunden; Test: pakete/windows/test_gplwaechter.sh, läuft in "make test").
# Reine Bash-Funktionen ohne Seiteneffekte, damit sie sich mit Beispielangaben prüfen lassen.

# Dateinamen, die nie ins Paket dürfen (FFmpeg und seine GPL-Anhängsel). Muster ohne Beachtung der Groß-/Kleinschreibung.
GESPERRTE_DATEIEN=(avcodec* avformat* avutil* swscale* swresample* postproc* libx264* libx265* ffmpegmediaplugin* libxvidcore* libmp3lame* libgnutls*)

# datei_gesperrt <relativer Pfad>: Exit 0, wenn der Dateiname (ohne Ordner) auf der Sperrliste steht
datei_gesperrt() {
    local name="${1##*/}" muster
    name="${name,,}"
    for muster in "${GESPERRTE_DATEIEN[@]}"; do
        # auch mit "lib"-Vorsatz (libavcodec-61.dll) prüfen
        # shellcheck disable=SC2053
        [[ "$name" == $muster || "$name" == lib$muster ]] && return 0
    done
    return 1
}

# lizenz_teil_ok <Teil>: ein einzelner Lizenzbezeichner (ohne OR/AND). ok = keine GPL, LGPL oder mit Ausnahme (WITH …)
lizenz_teil_ok() {
    local t="${1//[()]/}"
    t="${t#"${t%%[![:space:]]*}"}"; t="${t%"${t##*[![:space:]]}"}"
    [ -n "$t" ] || return 1
    [[ "$t" =~ GPL ]] || return 0           # keine (A/L)GPL: unkritisch
    [[ "$t" =~ [[:space:]]WITH[[:space:]] ]] && return 0
    [[ "$t" =~ ^LGPL ]] && return 0
    return 1                                # reine GPL (oder AGPL)
}

# lizenz_alternative_ok <Ausdruck ohne OR>: alle AND-Bestandteile müssen ok sein ("LGPL-2.1 AND GPL-2.0" -> nein)
lizenz_alternative_ok() {
    local teil
    while IFS= read -r teil; do
        lizenz_teil_ok "$teil" || return 1
    done <<< "${1// AND /$'\n'}"
}

# lizenz_eintrag_ok <Eintrag>: ein SPDX-Ausdruck; bei "A OR B" genügt eine Alternative ohne reine GPL
lizenz_eintrag_ok() {
    local alt
    while IFS= read -r alt; do
        lizenz_alternative_ok "$alt" && return 0
    done <<< "${1// OR /$'\n'}"
    return 1
}

# lizenz_ok <Lizenzangabe aus pacman -Qi>: mehrere Einträge sind durch zwei Leerzeichen getrennt und gelten ALLE
# (nicht als Wahl). Leer oder "None" -> Abbruch. Exit 0 = zulässig.
lizenz_ok() {
    local lic="${1//spdx:/}" eintrag
    lic="${lic#"${lic%%[![:space:]]*}"}"; lic="${lic%"${lic##*[![:space:]]}"}"
    [ -n "$lic" ] && [ "${lic,,}" != "none" ] || return 1
    while IFS= read -r eintrag; do
        [ -n "$eintrag" ] || continue
        [ "${eintrag,,}" != "none" ] || return 1
        [[ "$eintrag" == documentation:* ]] && continue   # nur für die Dokumentation des Pakets (z. B. libiconv), nicht für die DLL
        lizenz_eintrag_ok "$eintrag" || return 1
    done < <(printf '%s\n' "$lic" | awk '{gsub(/  +/, "\n"); print}')
    return 0
}

# lizenz_ausnahme <Paketname> [Datei]: Exit 0, wenn das Paket in pakete/windows/gpl-ausnahmen.txt steht (je Zeile "Paket  # Begründung").
# Nur für Pakete, deren Lizenzangabe ein Sammelwert aus Bibliothek, Werkzeugen und Beispielen ist und deren gepackte DLLs
# von Hand als LGPL bzw. LGPL mit Ausnahme geprüft wurden. Die Sperrliste und "None"/leer gelten auch für sie.
lizenz_ausnahme() {
    local liste="${GPL_AUSNAHMEN:-$(dirname "${BASH_SOURCE[0]}")/gpl-ausnahmen.txt}"
    [ -f "$liste" ] || return 1
    grep -q -E "^${1//./\\.}([[:space:]]|\$)" "$liste"
}
