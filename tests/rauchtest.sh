#!/bin/bash
# Live-Rauchtest in der laufenden Plasma-Sitzung: startet schneefall, ruft jede
# D-Bus-Methode auf und prüft, dass das Programm danach noch lebt.
# (Regression 18.11.: ein const-Slot mit QString-Rückgabe ließ QtDBus abstürzen.)
cd "$(dirname "$0")/.." || exit 1
P=./build/schneefall
if busctl --user status org.schneefall >/dev/null 2>&1; then echo "schneefall läuft schon – erst beenden"; exit 1; fi
setsid -f $P >/dev/null 2>&1
for i in $(seq 50); do busctl --user status org.schneefall >/dev/null 2>&1 && break; sleep 0.1; done
fehler=0
for m in Status Fensterliste Umschalten Umschalten Abschuetteln; do
    busctl --user call org.schneefall / org.schneefall $m >/dev/null 2>&1
    sleep 0.3
    if busctl --user status org.schneefall >/dev/null 2>&1; then echo "PASS  $m"; else echo "FAIL! $m (Absturz)"; fehler=1; break; fi
done
$P --beenden >/dev/null 2>&1
exit $fehler
