// Schneefall – Fenster: schickt die Liste der sichtbaren Fenster an den
// D-Bus-Dienst org.schneefall (Programm ~/Schneefall). Läuft schneefall nicht,
// gehen die Aufrufe einfach ins Leere.

const DIENST = "org.schneefall";

function istRelevant(w) {
    if (!w || w.deleted) return false;
    // die Schnee-Ebenen selbst (Layer-Shell, ohne Titel, ohne Eingabe) nie melden;
    // das Steuerfenster (Titel "Schneefall") zählt wie ein normales Fenster
    if (w.resourceClass === "schneefall" && (!w.wantsInput || w.caption === "")) return false;
    if (w.desktopWindow || w.popupWindow || w.tooltip || w.notification
        || w.criticalNotification || w.onScreenDisplay || w.splash
        || w.dropdownMenu || w.popupMenu || w.menu || w.comboBox || w.dndIcon) return false;
    if (!(w.normalWindow || w.dialog || w.dock || w.utility)) return false;
    return true;
}

function aufAktuellerArbeitsflaeche(w) {
    if (w.dock) return true;
    const d = workspace.currentDesktop;
    if (!w.onAllDesktops && w.desktops && d && !w.desktops.some(x => x === d || (x && d && x.id === d.id))) return false;
    if (w.activities && w.activities.length > 0 && workspace.currentActivity
        && w.activities.indexOf(workspace.currentActivity) < 0) return false;
    return true;
}

function liste() {
    const stapel = workspace.stackingOrder;
    const aus = [];
    for (let i = 0; i < stapel.length; ++i) {
        const w = stapel[i];
        if (!istRelevant(w)) continue;
        const g = w.frameGeometry;
        aus.push({
            id: String(w.internalId),
            x: g.x, y: g.y, w: g.width, h: g.height,
            s: i,
            min: !!w.minimized || !!w.hidden || !aufAktuellerArbeitsflaeche(w),
            fs: !!w.fullScreen,
            akt: !!w.active,
            dock: !!w.dock,
        });
    }
    return aus;
}

let letzter = "";
function senden(immer) {
    const text = JSON.stringify(liste());
    if (!immer && text === letzter) return;
    letzter = text;
    callDBus(DIENST, "/", DIENST, "Fenster", text);
}

// Änderungen kurz sammeln (beim Ziehen kommen sehr viele)
const bald = new QTimer();
bald.singleShot = true;
bald.interval = 15;
bald.timeout.connect(() => senden(false));
function geaendert() { if (!bald.active) bald.start(); }

// alle 2 s vollständig senden, damit ein frisch gestartetes schneefall den Stand bekommt
const puls = new QTimer();
puls.interval = 2000;
puls.timeout.connect(() => senden(true));
puls.start();

function verbinden(w) {
    for (const s of ["frameGeometryChanged", "minimizedChanged", "fullScreenChanged", "desktopsChanged", "hiddenChanged", "stackingOrderChanged", "activeChanged"])
        if (w[s] && w[s].connect) w[s].connect(geaendert);
}

// Signale nur verbinden, wenn diese KWin-Version sie kennt
function wenn(obj, name, fn) {
    if (obj[name] && obj[name].connect) obj[name].connect(fn);
    else print("Schneefall: Signal fehlt:", name);
}
wenn(workspace, "windowAdded", w => { verbinden(w); geaendert(); });
wenn(workspace, "windowRemoved", geaendert);
wenn(workspace, "windowActivated", geaendert);
wenn(workspace, "currentDesktopChanged", geaendert);
wenn(workspace, "currentActivityChanged", geaendert);
wenn(workspace, "screensChanged", geaendert);

for (const w of workspace.windowList()) verbinden(w);
senden(true);
