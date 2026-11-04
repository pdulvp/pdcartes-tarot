// Capture d'écran de la table à 5 joueurs pendant l'enchère.
#include "MainWindow.h"
#include "TablePicker.h"
#include "TablePage.h"
#include "TableView.h"
#include "Tarot.h"

#include <QApplication>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <cstdio>
#include <cstdlib>

static void pump(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        QThread::msleep(15);
    }
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    const int seats = argc > 1 ? atoi(argv[1]) : 5;
    MainWindow w;
    w.resize(1100, 720);
    w.show();
    pump(300);
    auto *picker = w.findChild<TablePicker *>();
    picker->setSeats(seats);
    QMetaObject::invokeMethod(picker, "tableChosen", Q_ARG(QString, QStringLiteral("tapis")));
    pump(2500);   // laisser la distribution se jouer

    auto *page = w.findChild<TablePage *>();
    auto *view = page->findChild<TableView *>();
    auto *game = page->findChild<Tarot *>();
    // Le joueur annonce la petite, puis on laisse les autres parler.
    for (QPushButton *b : page->findChildren<QPushButton *>()) {
        if (b->isVisible() && b->isEnabled() && b->text().contains(QStringLiteral("etite"))) {
            b->click();
            break;
        }
    }
    // attendre que plusieurs aient parlé, tout en restant dans l'enchère
    for (int i = 0; i < 40; ++i) {
        pump(150);
        int said = 0;
        for (int at = 0; at < game->seatCount(); ++at)
            if (!game->seat(at).said.isEmpty()) ++said;
        if (game->phase() == Phase::Bidding && said >= 3) break;
    }

    std::printf("sièges=%d phase=%d\n", game->seatCount(), int(game->phase()));
    const QPixmap shot = view->grab();
    const QString out = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                 : QStringLiteral("bid-%1.png").arg(seats);
    shot.save(out);
    std::printf("capture %dx%d -> %s (phase=%d)\n", shot.width(), shot.height(),
                qPrintable(out), int(game->phase()));
    return 0;
}
