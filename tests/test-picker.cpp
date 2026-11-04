// Le menu d'ouverture (#17) : « La table » (3/4/5 joueurs) en premier, puis
// « Les joueurs », puis « La partie » ; les tuiles de sièges assez hautes pour
// que leur blurb ne soit pas tronqué.
#include "MainWindow.h"
#include "TablePicker.h"
#include "TileButton.h"

#include <QApplication>
#include <QLabel>
#include <QThread>
#include <QElapsedTimer>
#include <cstdio>
#include <algorithm>

static void pump(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        QThread::msleep(10);
    }
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1100, 760);
    w.show();
    pump(400);

    auto *picker = w.findChild<TablePicker *>();
    if (!picker) { std::printf("ECHEC: pas de picker\n"); return 1; }

    int failures = 0;

    // Les trois titres de section, triés par position verticale.
    QList<QPair<int, QString>> found;
    for (QWidget *section : picker->findChildren<QWidget *>(QStringLiteral("feltSection"))) {
        auto *label = section->findChild<QLabel *>(QStringLiteral("feltSectionLabel"));
        if (!label) continue;
        found.append({section->mapTo(picker, QPoint(0, 0)).y(), label->text()});
    }
    std::sort(found.begin(), found.end(),
              [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                  return a.first < b.first;
              });
    QStringList titles;
    for (const auto &p : found) titles.append(p.second);
    std::printf("sections (haut -> bas): %s\n", qPrintable(titles.join(QStringLiteral(" | "))));
    const QStringList want = {QStringLiteral("La table"), QStringLiteral("Les joueurs"),
                              QStringLiteral("La partie")};
    if (titles != want) {
        std::printf("ECHEC ordre des sections: %s\n", qPrintable(titles.join(" | ")));
        ++failures;
    }

    // Les tuiles de sièges : trois, « 3/4/5 joueurs », assez hautes.
    QList<TileButton *> seatTiles;
    for (TileButton *t : picker->findChildren<TileButton *>()) {
        if (t->text().contains(QStringLiteral("joueurs"))) seatTiles.append(t);
    }
    if (seatTiles.size() != 3) {
        std::printf("ECHEC %d tuiles de sièges (attendu 3)\n", seatTiles.size());
        ++failures;
    }
    for (TileButton *t : seatTiles) {
        std::printf("  tuile [%s] %dx%d\n", qPrintable(t->text()), t->width(),
                    t->height());
        if (t->height() < 140) {
            std::printf("ECHEC tuile %s trop basse (%d)\n", qPrintable(t->text()), t->height());
            ++failures;
        }
    }

    // Les tuiles de partie : une par table offerte.
    int gameTiles = 0;
    for (TileButton *t : picker->findChildren<TileButton *>()) {
        if (!t->text().contains(QStringLiteral("joueurs"))) ++gameTiles;
    }
    std::printf("tuiles de partie: %d\n", gameTiles);
    if (gameTiles < 4) { std::printf("ECHEC trop peu de tuiles de partie\n"); ++failures; }

    std::printf(failures == 0 ? "TOUT OK\n" : "%d ECHEC\n", failures);
    return failures == 0 ? 0 : 1;
}
