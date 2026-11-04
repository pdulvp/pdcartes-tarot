// Pilote UI hors écran : la partie démarre au nombre de joueurs demandé,
// le bouton Petite est disponible, puis les boutons d'appel à 5 joueurs ;
// on les clique par programme.
#include "MainWindow.h"
#include "TablePicker.h"
#include "TablePage.h"
#include "Tarot.h"

#include <QApplication>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <cstdio>
#include <cstdlib>

static QPushButton *findButton(QWidget *root, const QString &needle) {
    for (QPushButton *b : root->findChildren<QPushButton *>()) {
        if (b->text().contains(needle) && b->isVisible()) return b;
    }
    return nullptr;
}

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
    MainWindow w;
    w.resize(1000, 700);
    w.show();
    pump(300);

    auto *picker = w.findChild<TablePicker *>();
    if (!picker) { std::printf("ECHEC: pas de picker\n"); return 1; }
    const int seats = argc > 1 ? atoi(argv[1]) : 5;
    picker->setSeats(seats);
    std::printf("picker seats=%d\n", picker->seats());

    QMetaObject::invokeMethod(picker, "tableChosen", Q_ARG(QString, QStringLiteral("tapis")));
    pump(600);

    auto *page = w.findChild<TablePage *>();
    if (!page) { std::printf("ECHEC: pas de TablePage\n"); return 1; }
    auto *game = page->findChild<Tarot *>();
    if (!game) { std::printf("ECHEC: pas de Tarot\n"); return 1; }
    std::printf("sièges=%d phase=%d tour=%d\n", game->seatCount(), int(game->phase()), game->turn());

    // Le joueur humain prend la petite dès que le bouton est dispo.
    bool bidDone = false, callDone = false;
    for (int step = 0; step < 200; ++step) {
        pump(120);
        std::printf("step %d: phase=%d tour=%d preneur=%d appelé=%d\n", step, int(game->phase()),
                    game->turn(), game->taker(), game->callee());
        if (step == 2) {
            std::printf("  page visible=%d, boutons de la page:\n", page->isVisible());
            for (QPushButton *b : page->findChildren<QPushButton *>()) {
                std::printf("    text=[%s] visible=%d enabled=%d\n", qPrintable(b->text()),
                            b->isVisible(), b->isEnabled());
            }
        }
        if (!bidDone) {
            if (auto *b = findButton(page, QStringLiteral("etite"))) {
                if (b->isEnabled()) {
                    std::printf("  -> clic Petite\n");
                    b->click();
                    bidDone = true;
                } else {
                    std::printf("  bouton Petite visible mais désactivé\n");
                }
            } else {
                std::printf("  bouton Petite absent (phase=%d tour=%d)\n", int(game->phase()),
                            game->turn());
            }
        }
        if (!callDone && game->phase() == Phase::Appel) {
            if (auto *b = findButton(page, QStringLiteral("Appeler"))) {
                if (b->isEnabled()) {
                    std::printf("  -> clic %s\n", qPrintable(b->text()));
                    b->click();
                    callDone = true;
                } else {
                    std::printf("  bouton d'appel visible mais désactivé\n");
                }
            } else {
                std::printf("  boutons d'appel absents en phase Appel (tour=%d)\n", game->turn());
            }
        }
        if (callDone && game->phase() != Phase::Appel) break;
        if (game->phase() == Phase::Chien || game->phase() == Phase::Ecart ||
            game->phase() == Phase::Playing)
            break;
    }
    std::printf("fin: phase=%d preneur=%d appelé=%d\n", int(game->phase()), game->taker(),
                game->callee());
    return 0;
}
