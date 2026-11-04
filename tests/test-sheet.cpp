// La fiche d'un adversaire (#16) : tempérament persisté par profil, remise à
// zéro de l'expérience seule, « changer ce qu'il est » qui efface l'expérience,
// et la boîte hors écran (édition annulée = rien d'écrit, validée = écrit).
#include "Profiles.h"
#include "RivalSheet.h"
#include "Rivals.h"
#include "SkillRow.h"

#include <QApplication>
#include <QDialog>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <cstdio>

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
    QCoreApplication::setOrganizationName("pdulvp");
    QCoreApplication::setApplicationName("pdcartes-tarot");
    int failures = 0;

    QSettings wipe;
    wipe.remove(QStringLiteral("profil"));
    wipe.sync();

    // 1. Sans tempérament écrit, rivalInnate retombe sur celui d'arrivée (hash).
    for (const Rival &r : rivals()) {
        if (rivalInnate(r.id) != rivalInnateAsArrived(r.id)) {
            std::printf("ECHEC repli hash pour %s\n", qPrintable(r.id));
            ++failures;
        }
    }
    std::printf("repli sur le tempérament d'arrivée ok\n");

    // 2. setRivalInnate avec les mêmes niveaux n'efface rien ; avec un niveau
    //    changé, le tempérament est écrit et l'expérience remise à zéro.
    noteRivalDonne(QStringLiteral("lulu"), true, true, true);
    noteRivalDonne(QStringLiteral("lulu"), true, false, false);
    const int xpBefore = learningFor(QStringLiteral("lulu")).xp;
    if (xpBefore <= 0) { std::printf("ECHEC xp avant=0\n"); ++failures; }
    const Innates same = rivalInnate(QStringLiteral("lulu"));
    setRivalInnate(QStringLiteral("lulu"), same);
    if (learningFor(QStringLiteral("lulu")).xp != xpBefore) {
        std::printf("ECHEC setRivalInnate identique a effacé l'xp\n"); ++failures;
    }
    Innates diff = same;
    diff.set(Innate::Audace, (same.at(Innate::Audace) + 1) % (kInnateTop + 1));
    setRivalInnate(QStringLiteral("lulu"), diff);
    const Learning after = learningFor(QStringLiteral("lulu"));
    if (rivalInnate(QStringLiteral("lulu")) != diff) {
        std::printf("ECHEC tempérament non persisté\n"); ++failures;
    }
    if (after.xp != rivalById(QStringLiteral("lulu")).startXp || after.pools.total() != after.xp) {
        std::printf("ECHEC xp non remise au départ (xp=%d pools=%d)\n", after.xp,
                    after.pools.total());
        ++failures;
    }
    std::printf("setRivalInnate: identique conserve l'xp (%d), changé écrit + remet à zéro (xp=%d)\n",
                xpBefore, after.xp);

    // 3. resetRivalExperience garde le nom et le tempérament.
    setRivalName(QStringLiteral("lulu"), QStringLiteral("Loulou"));
    noteRivalDonne(QStringLiteral("lulu"), true, true, true);
    resetRivalExperience(QStringLiteral("lulu"));
    if (learningFor(QStringLiteral("lulu")).xp != rivalById(QStringLiteral("lulu")).startXp) {
        std::printf("ECHEC resetRivalExperience xp\n"); ++failures;
    }
    if (rivalName(QStringLiteral("lulu")) != QStringLiteral("Loulou")) {
        std::printf("ECHEC resetRivalExperience a effacé le nom\n"); ++failures;
    }
    if (rivalInnate(QStringLiteral("lulu")) != diff) {
        std::printf("ECHEC resetRivalExperience a effacé le tempérament\n"); ++failures;
    }
    std::printf("resetRivalExperience: xp à zéro, nom et tempérament gardés\n");

    // 4. La fiche hors écran : 8 lignes, édition annulée n'écrit rien,
    //    édition validée écrit le tempérament et efface l'xp.
    {
        noteRivalDonne(QStringLiteral("oriane"), true, true, true);
        const int xpOriane = learningFor(QStringLiteral("oriane")).xp;
        const Innates before = rivalInnate(QStringLiteral("oriane"));

        RivalSheet sheet(QStringLiteral("oriane"));
        sheet.show();
        pump(100);
        const QList<SkillRow *> rows = sheet.findChildren<SkillRow *>();
        if (rows.size() != 8) {
            std::printf("ECHEC fiche: %d lignes au lieu de 8\n", rows.size());
            ++failures;
        }
        // Bouton « Changer » (icône d'en-tête de la colonne tempérament).
        QToolButton *change = nullptr;
        for (QToolButton *b : sheet.findChildren<QToolButton *>()) {
            if (b->toolTip().contains(QStringLiteral("Changer ce qu'il est"))) change = b;
        }
        if (!change) { std::printf("ECHEC pas de bouton Changer\n"); ++failures; }
        else {
            change->click();
            pump(50);
            // Clic sur la pastille suivante de la première ligne (Audace).
            SkillRow *row = rows.first();
            const int target = (row->level() + 1) % (kInnateTop + 1);
            const QPointF pos(112.0 + target * 18.0 + 5.5, row->height() / 2.0);
            QMouseEvent press(QEvent::MouseButtonPress, pos, row->mapToGlobal(pos.toPoint()),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(row, &press);
            pump(50);
            if (row->level() != target) {
                std::printf("ECHEC pastille cliquée: niveau=%d attendu=%d\n", row->level(), target);
                ++failures;
            }
            // Annuler : rien n'est écrit.
            QPushButton *cancel = nullptr;
            for (QPushButton *b : sheet.findChildren<QPushButton *>()) {
                if (b->text() == QStringLiteral("Annuler") && b->isVisible()) cancel = b;
            }
            if (!cancel) { std::printf("ECHEC pas de bouton Annuler visible\n"); ++failures; }
            else {
                cancel->click();
                pump(50);
                if (rivalInnate(QStringLiteral("oriane")) != before ||
                    learningFor(QStringLiteral("oriane")).xp != xpOriane) {
                    std::printf("ECHEC annuler a écrit quelque chose\n"); ++failures;
                }
            }
            // Refaire l'édition, puis valider en répondant « Changer » à la
            // confirmation.
            change->click();
            pump(50);
            QMouseEvent press2(QEvent::MouseButtonPress, pos, row->mapToGlobal(pos.toPoint()),
                               Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(row, &press2);
            pump(50);
            // La confirmation arrive en modal : on y répond oui.
            QTimer::singleShot(150, [] {
                QWidget *modal = QApplication::activeModalWidget();
                if (!modal) return;
                for (QPushButton *b : modal->findChildren<QPushButton *>()) {
                    if (b->text() == QStringLiteral("Changer")) { b->click(); return; }
                }
            });
            QPushButton *commit = nullptr;
            for (QPushButton *b : sheet.findChildren<QPushButton *>()) {
                if (b->text() == QStringLiteral("Changer et réapprendre") && b->isVisible())
                    commit = b;
            }
            if (!commit) { std::printf("ECHEC pas de bouton de validation\n"); ++failures; }
            else {
                commit->click();
                pump(400);
                Innates expected = before;
                expected.set(Innate::Audace, target);
                if (rivalInnate(QStringLiteral("oriane")) != expected) {
                    std::printf("ECHEC validation: tempérament non écrit\n"); ++failures;
                }
                if (learningFor(QStringLiteral("oriane")).xp !=
                    rivalById(QStringLiteral("oriane")).startXp) {
                    std::printf("ECHEC validation: xp non remise au départ\n"); ++failures;
                }
            }
        }
        std::printf("fiche: 8 lignes, annuler sans effet, valider écrit tempérament + remet xp\n");
    }

    std::printf(failures == 0 ? "TOUT OK\n" : "%d ECHEC\n", failures);
    return failures == 0 ? 0 : 1;
}
