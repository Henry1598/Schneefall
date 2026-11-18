// Prüft das KWin-Skript ohne KWin: lädt main.js mit nachgebauten KWin-Objekten.
// Die Fenstereigenschaften stammen aus einem echten Mitschnitt (Plasma 6.8.3, 18.11.).
// Aufruf: node tests/test_kwin.js [pfad/zu/main.js]
"use strict";
const fs = require("fs");
const pfad = process.argv[2] || __dirname + "/../kwin/contents/code/main.js";
const quelle = fs.readFileSync(pfad, "utf8");

function signal() { return { connect() {} }; }
function fenster(p) {
    const g = { x: p.x, y: p.y, width: p.w, height: p.h };
    return Object.assign({
        deleted: false, minimized: false, hidden: false, fullScreen: false, active: false,
        normalWindow: true, dialog: false, dock: false, utility: false, desktopWindow: false,
        popupWindow: false, tooltip: false, notification: false, criticalNotification: false,
        onScreenDisplay: false, splash: false, dropdownMenu: false, popupMenu: false, menu: false,
        comboBox: false, dndIcon: false, onAllDesktops: true, desktops: [], activities: [],
        wantsInput: true, managed: true, internalId: "{" + p.name + "}", frameGeometry: g,
        frameGeometryChanged: signal(), minimizedChanged: signal(), fullScreenChanged: signal(),
    }, p);
}

function lauf(fensterListe) {
    const gesendet = [];
    class QTimer { constructor() { this.timeout = signal(); this.active = false; } start() {} stop() {} }
    const workspace = {
        stackingOrder: fensterListe, windowList: () => fensterListe,
        currentDesktop: { id: "d1" }, currentActivity: "",
        windowAdded: signal(), windowRemoved: signal(), windowActivated: signal(),
        currentDesktopChanged: signal(), screensChanged: signal(),
    };
    const callDBus = (dienst, pfad, iface, methode, text) => gesendet.push({ methode, liste: JSON.parse(text) });
    new Function("workspace", "QTimer", "callDBus", "print", quelle)(workspace, QTimer, callDBus, () => {});
    return gesendet[gesendet.length - 1].liste;
}

let fehler = 0;
function pruefe(bedingung, text) {
    if (bedingung) console.log("PASS  " + text);
    else { console.log("FAIL! " + text); ++fehler; }
}

// Mitschnitt: Schnee-Ebene auf DP-3 (Layer-Shell, Bereich "schneefall")
const ebene = fenster({ name: "ebene", resourceClass: "schneefall", caption: "", wantsInput: false, skipTaskbar: true,
                        x: 1080, y: 1440, w: 3413.33, h: 1440 });
const steuer = fenster({ name: "steuer", resourceClass: "schneefall", caption: "Schneefall", x: 2000, y: 1600, w: 400, h: 300 });
const konsole = fenster({ name: "konsole", resourceClass: "org.kde.konsole", caption: "Konsole", x: 3087.25, y: 1657.97, w: 1383.3, h: 1031.3 });
const steam = fenster({ name: "steam", resourceClass: "steam", caption: "Steam", x: 1707.3, y: 1700, w: 1219.3, h: 678 }); // XWayland
const leiste = fenster({ name: "leiste", resourceClass: "plasmashell", caption: "", normalWindow: false, dock: true,
                         wantsInput: false, onAllDesktops: false, x: 2218, y: 2824, w: 1137, h: 56 });
const hintergrund = fenster({ name: "hg", resourceClass: "plasmashell", caption: "", normalWindow: false, desktopWindow: true,
                              x: 1080, y: 1440, w: 3413.33, h: 1440 });

const liste = lauf([hintergrund, steam, konsole, steuer, leiste, ebene]);
const ids = liste.map(w => w.id);
pruefe(!ids.includes("{ebene}"), "eigene Schnee-Ebenen werden nicht als Fenster gemeldet");
pruefe(!ids.includes("{hg}"), "Arbeitsflächen-Hintergrund wird nicht gemeldet");
pruefe(ids.includes("{steuer}"), "Steuerfenster zählt als normales Fenster");
pruefe(ids.includes("{konsole}") && ids.includes("{steam}"), "Wayland- und XWayland-Fenster werden gemeldet");
const l = liste.find(w => w.id === "{leiste}");
pruefe(l && l.dock && !l.min, "Taskleiste wird als Dock gemeldet");
const k = liste.find(w => w.id === "{konsole}");
pruefe(k && k.x === 3087.25 && k.y === 1657.97 && typeof k.s === "number", "Geometrie und Stapelplatz kommen unverändert an");

process.exit(fehler ? 1 : 0);
