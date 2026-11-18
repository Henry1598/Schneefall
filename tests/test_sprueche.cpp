// Sprüche: Datei, Auswahl, Eskalation, Ausdrucksweise, Ersatzsätze, Einstellung
#include <QtTest>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include "einstellungen.h"
#include "sprueche.h"

using namespace schnee;

namespace {
const Sprecher SPRECHER[] = {Sprecher::Fahrer, Sprecher::Pilot};
const Anlass FAHRER_ANLAESSE[] = {Anlass::Treffer, Anlass::Streik, Anlass::BeleidigtAbflug, Anlass::Gruss};
const Anlass PILOT_ANLAESSE[] = {Anlass::HeliTreffer, Anlass::HeliAbholen, Anlass::HeliAbflug};
QByteArray dateiInhalt() {
    QFile f(QStringLiteral(SPRUECHE_TSV));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
// normalisiert für den Dublettenvergleich: Kleinschreibung, nur Buchstaben und Ziffern
QString norm(QString t) {
    t = t.toLower();
    QString o;
    for (QChar c : t) if (c.isLetterOrNumber()) o += c;
    return o;
}
}

class TestSprueche : public QObject {
    Q_OBJECT
private slots:
    void dateiHatMindestens1000GueltigeZeilen() {
        Sprueche s;
        QCOMPARE_GE(s.ladeText(dateiInhalt()), 1000);
        QVERIFY(!s.eingebaut());
        // keine Zeile wurde als ungültig verworfen
        int zeilen = 0;
        for (const QByteArray &z : dateiInhalt().split('\n')) if (!z.trimmed().isEmpty() && !z.startsWith('#')) ++zeilen;
        QCOMPARE(s.anzahl(), zeilen);
    }
    void ungefaehrSechshundertFahrerVierhundertPilot() {
        Sprueche s;
        s.ladeText(dateiInhalt());
        int fahrer = 0, pilot = 0;
        for (Anlass a : FAHRER_ANLAESSE) for (int st = 1; st <= 5; ++st) fahrer += s.anzahl(Sprecher::Fahrer, a, st);
        for (Anlass a : PILOT_ANLAESSE) for (int st = 1; st <= 5; ++st) pilot += s.anzahl(Sprecher::Pilot, a, st);
        QCOMPARE_GE(fahrer, 590);
        QCOMPARE_GE(pilot, 390);
        QCOMPARE(fahrer + pilot, s.anzahl()); // Fahrer sagt nichts vom Heli und umgekehrt
    }
    void keineDublettenAuchNichtNurGrossKleinOderZeichensetzung() {
        QSet<QString> gesehen;
        for (const QByteArray &z : dateiInhalt().split('\n')) {
            if (z.trimmed().isEmpty() || z.startsWith('#')) continue;
            const QStringList t = QString::fromUtf8(z).split('\t');
            QCOMPARE(t.size(), 4);
            const QString n = norm(t[3]);
            QVERIFY2(!gesehen.contains(n), qPrintable(t[3]));
            gesehen.insert(n);
        }
    }
    void jedeAnlassStufeKombinationHatMindestens15() {
        Sprueche s;
        s.ladeText(dateiInhalt());
        for (Anlass a : FAHRER_ANLAESSE) for (int st = 1; st <= 5; ++st)
            QVERIFY2(s.anzahl(Sprecher::Fahrer, a, st) >= 15, qPrintable(QStringLiteral("fahrer %1 stufe %2").arg(int(a)).arg(st)));
        for (Anlass a : PILOT_ANLAESSE) for (int st = 1; st <= 5; ++st)
            QVERIFY2(s.anzahl(Sprecher::Pilot, a, st) >= 15, qPrintable(QStringLiteral("pilot %1 stufe %2").arg(int(a)).arg(st)));
    }
    void langeZeilenPassenInDieSprechblase() {
        for (const QByteArray &z : dateiInhalt().split('\n')) {
            if (z.trimmed().isEmpty() || z.startsWith('#')) continue;
            const QStringList t = QString::fromUtf8(z).split('\t');
            QVERIFY2(t[3].size() <= 70, qPrintable(t[3]));
        }
    }
    void ungueltigeZeilenWerdenIgnoriert() {
        Sprueche s;
        const QByteArray tsv = "# Kommentar\n"
                               "fahrer\ttreffer\t1\tOk eins\n"
                               "fahrer\ttreffer\t1\tOk eins\n"           // Dublette
                               "fahrer\ttreffer\t9\tStufe falsch\n"
                               "kater\ttreffer\t1\tSprecher falsch\n"
                               "fahrer\tquatsch\t1\tAnlass falsch\n"
                               "fahrer\ttreffer\t1\n"                      // Spalte fehlt
                               "fahrer\ttreffer\t2\t\n"                    // leerer Text
                               "pilot\theli_treffer\t3\tOk zwei\n";
        QCOMPARE(s.ladeText(tsv), 2);
        QCOMPARE(s.anzahl(), 2);
    }
    void fallbackOhneDateiNutztEingebauteSaetze() {
        Sprueche s;
        QCOMPARE(s.ladeDatei(QStringLiteral("/gibt/es/nicht.tsv")), 0);
        QCOMPARE(s.ladeText("kaputt\nnur Müll\n"), 0);
        QVERIFY(s.eingebaut());
        QCOMPARE_GT(s.anzahl(), 0);
        for (Sprecher sp : SPRECHER)
            for (Anlass a : sp == Sprecher::Fahrer ? QList<Anlass>{Anlass::Treffer, Anlass::Streik, Anlass::BeleidigtAbflug, Anlass::Gruss}
                                                   : QList<Anlass>{Anlass::HeliTreffer, Anlass::HeliAbholen, Anlass::HeliAbflug}) {
                s.setzeMaxStufe(5);
                for (int st = 1; st <= 5; ++st) QVERIFY(!s.waehle(sp, a, st).isEmpty());
            }
        // eingebaute Sätze bleiben auch nach misslungenem Laden erhalten, nach gutem Laden werden sie ersetzt
        QVERIFY(s.ladeText(dateiInhalt()) > 0);
        QVERIFY(!s.eingebaut());
    }
    void ressourceIstEingebettet() {
        Sprueche s;
        QCOMPARE_GE(s.ladeDatei(QStringLiteral(":/daten/sprueche.tsv")), 1000);
    }
    void stufeWachstMitTreffernUndIstGedeckelt() {
        QCOMPARE(spruchStufeFuerTreffer(1, 5), 1);
        QCOMPARE(spruchStufeFuerTreffer(2, 5), 2);
        QCOMPARE(spruchStufeFuerTreffer(5, 5), 5);
        QCOMPARE(spruchStufeFuerTreffer(99, 5), 5);
        QCOMPARE(spruchStufeFuerTreffer(99, 3), 3);
        QCOMPARE(spruchStufeFuerTreffer(0, 3), 1);
        QCOMPARE(spruchStufeFuerTreffer(4, 99), 4); // Einstellung wird selbst begrenzt
        QCOMPARE(spruchStufeFuerTreffer(4, -3), 1);
    }
    void einstellungBegrenztDieStufe() {
        // jeder Satz trägt seine Stufe; so lässt sich prüfen, aus welcher Stufe gezogen wurde
        QHash<QString, int> stufeVon;
        for (const QByteArray &z : dateiInhalt().split('\n')) {
            const QStringList t = QString::fromUtf8(z).split('\t');
            if (t.size() == 4) stufeVon.insert(t[0] + t[3], t[2].toInt());
        }
        for (int max = 1; max <= 5; ++max) {
            Sprueche s;
            s.ladeText(dateiInhalt());
            s.setzeMaxStufe(max);
            s.setzeSamen(7);
            for (int i = 0; i < 300; ++i) {
                const QString t = s.waehle(Sprecher::Fahrer, Anlass::Treffer, 5); // verlangt wird immer das Äußerste
                QVERIFY2(stufeVon.value(QStringLiteral("fahrer") + t, 99) <= max, qPrintable(t));
            }
            for (int i = 0; i < 300; ++i) {
                const QString t = s.waehleBeiTreffer(Sprecher::Pilot, Anlass::HeliTreffer, 1 + i % 8);
                QVERIFY2(stufeVon.value(QStringLiteral("pilot") + t, 99) <= max, qPrintable(t));
            }
        }
    }
    void eskalationErsterTrefferFreundlichDannHoeher() {
        QHash<QString, int> stufeVon;
        for (const QByteArray &z : dateiInhalt().split('\n')) {
            const QStringList t = QString::fromUtf8(z).split('\t');
            if (t.size() == 4) stufeVon.insert(t[0] + t[3], t[2].toInt());
        }
        Sprueche s;
        s.ladeText(dateiInhalt());
        s.setzeMaxStufe(5);
        for (int treffer = 1; treffer <= 7; ++treffer)
            QCOMPARE(stufeVon.value(QStringLiteral("fahrer") + s.waehleBeiTreffer(Sprecher::Fahrer, Anlass::Treffer, treffer)),
                     std::min(treffer, 5));
    }
    void keineWiederholungInnerhalbDesFensters() {
        Sprueche s;
        s.ladeText(dateiInhalt());
        s.setzeMaxStufe(5);
        for (Sprecher sp : SPRECHER) {
            const Anlass a = sp == Sprecher::Fahrer ? Anlass::Treffer : Anlass::HeliTreffer;
            QCOMPARE_GE(s.anzahl(sp, a, 1), 1);
            // Alle Sätze über alle Stufen eines Anlasses: 200+ Möglichkeiten, Fenster 50
            QStringList verlauf;
            for (int i = 0; i < 400; ++i) {
                const QString t = s.waehle(sp, a, 1 + i % 5);
                const int von = std::max<int>(0, int(verlauf.size()) - SPRUCH_FENSTER);
                for (int k = von; k < verlauf.size(); ++k) QVERIFY2(verlauf[k] != t, qPrintable(QStringLiteral("Wiederholung bei %1: %2").arg(i).arg(t)));
                verlauf << t;
            }
        }
    }
    void kleinerPoolWiederholtNurWennNichtsAnderesBleibt() {
        Sprueche s;
        s.ladeText("fahrer\ttreffer\t1\tA\nfahrer\ttreffer\t1\tB\nfahrer\ttreffer\t1\tC\n");
        QSet<QString> d;
        QString vorher;
        for (int i = 0; i < 30; ++i) {
            const QString t = s.waehle(Sprecher::Fahrer, Anlass::Treffer, 1);
            QVERIFY(!t.isEmpty());
            if (i < 3) QVERIFY(!d.contains(t)); // erst alle drei, dann reihum
            d.insert(t);
            QVERIFY(t != vorher);
            vorher = t;
        }
        QCOMPARE(d.size(), 3);
    }
    void fehlendeStufeFaelltAufNiedrigereZurueckNieDarueber() {
        Sprueche s;
        s.ladeText("fahrer\tstreik\t1\tNett\nfahrer\tstreik\t4\tDerb\n");
        s.setzeMaxStufe(3);
        QCOMPARE(s.waehle(Sprecher::Fahrer, Anlass::Streik, 3), QStringLiteral("Nett"));
        QCOMPARE(s.waehle(Sprecher::Fahrer, Anlass::Streik, 5), QStringLiteral("Nett"));
        QVERIFY(s.waehle(Sprecher::Fahrer, Anlass::Gruss, 1).isEmpty()); // für Gruß gibt es hier nichts
        Sprueche t;
        t.ladeText("fahrer\tstreik\t4\tDerb\n");
        t.setzeMaxStufe(2);
        QVERIFY(t.waehle(Sprecher::Fahrer, Anlass::Streik, 2).isEmpty()); // Stufe 4 ist nie erlaubt
    }
    void einstellungMitAbwaertskompatibilitaet() {
        Programmeinstellungen p = einstellungenAusText("menge=2\n");
        QCOMPARE(p.spruecheMaxStufe, 3);                       // alte Datei ohne Schlüssel: Standard 3
        QVERIFY(einstellungenBrauchtErgaenzung("menge=2\n"));
        QCOMPARE(einstellungenAusText("sprueche_max_stufe=5\n").spruecheMaxStufe, 5);
        QCOMPARE(einstellungenAusText("sprueche_max_stufe=99\n").spruecheMaxStufe, 5);
        QCOMPARE(einstellungenAusText("sprueche_max_stufe=0\n").spruecheMaxStufe, 1);
        QCOMPARE(einstellungenAusText("sprueche_max_stufe=quatsch\n").spruecheMaxStufe, 3);
        p.spruecheMaxStufe = 4;
        const std::string text = einstellungenAlsText(p);
        QVERIFY(text.find("\nsprueche_max_stufe=4\n") != std::string::npos);
        QCOMPARE(einstellungenAusText(text).spruecheMaxStufe, 4);
        QVERIFY(!einstellungenBrauchtErgaenzung(text));
    }
};

QTEST_GUILESS_MAIN(TestSprueche)
#include "test_sprueche.moc"
