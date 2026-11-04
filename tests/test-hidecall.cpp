// À 5 joueurs, l'appel reste caché (#12) : callRevealed reste faux tant que la
// carte appelée n'est pas jouée, vrai après ; le message d'appel ne nomme pas
// le partenaire avant, et la révélation est annoncée quand la carte est posée.
#include "Tarot.h"

#include <QCoreApplication>
#include <cstdio>

static QVector<SeatSpec> specsFor(int seats) {
    QVector<SeatSpec> specs;
    specs.append({QString(), QStringLiteral("Vous"), 0});
    for (int i = 1; i < seats; ++i) {
        specs.append({QStringLiteral("r%1").arg(i), QStringLiteral("J%1").arg(i), i * 40});
    }
    return specs;
}

// Joue une donne entière avec le siège 0 comme preneur. Renvoie le siège de
// l'appelé (-1 si seul). `msgs` récupère les messages `acted`.
static int playDonneHidden(Tarot &game, QVector<QPair<bool, QString>> &msgs, bool &revealedBeforeCalledCard,
                           bool &revealedAfterCalledCard, bool &calledCardSeen) {
    QObject::connect(&game, &Tarot::acted, &game,
                     [&msgs, &game](int, const QString &tale) {
                         msgs.append({game.callRevealed(), tale});
                     });
    int guard = 0;
    while (game.phase() == Phase::Bidding && guard++ < 20) {
        if (game.turn() == 0) game.makeBid(Contract::Petite);
        else game.pass();
    }
    if (game.taker() != 0) return -2;
    if (game.phase() != Phase::Appel) return -3;
    // Appelle la première couleur dont la carte appelée n'est ni chez le
    // preneur ni au chien (on ne voit pas le chien : on teste les mains).
    Suit suit = Suit::Pique;
    for (int s = 0; s < 4; ++s) {
        const Card called = game.calledCardFor(Suit(s), 0);
        if (!findCard(game.seat(0).hand, called.id)) { suit = Suit(s); break; }
    }
    QString why;
    if (!game.callKing(0, suit, &why)) {
        std::printf("  appel refusé : %s\n", qPrintable(why));
        return -4;
    }
    const int callee = game.callee();
    if (game.phase() == Phase::Chien) game.chienSeen();
    if (game.phase() == Phase::Ecart) {
        if (game.ecartNeeded()) {
            QVector<int> ids;
            for (const Card &c : game.seat(0).hand) {
                if (game.mayDiscard(c)) ids.append(c.id);
                if (ids.size() >= game.chienSize()) break;
            }
            if (!game.setEcart(ids, &why)) { std::printf("  écart refusé : %s\n", qPrintable(why)); return -5; }
        }
        game.finishEcart();
    }
    // Joue les plis en suivant les règles ; on note la révélation.
    guard = 0;
    revealedBeforeCalledCard = false;
    revealedAfterCalledCard = false;
    calledCardSeen = false;
    while (game.phase() == Phase::Playing || game.phase() == Phase::TrickOver) {
        if (++guard > 500) return -6;
        if (game.phase() == Phase::TrickOver) { game.sweepTrick(); continue; }
        const int at = game.turn();
        const QVector<int> ok = game.legalCards(at);
        if (ok.isEmpty()) return -7;
        const int cardId = ok.first();
        const Card *held = findCard(game.seat(at).hand, cardId);
        const Card played = held ? *held : Card{};
        const bool wasRevealed = game.callRevealed();
        if (!game.play(cardId)) return -8;
        const Card calledNow = game.calledCardFor(game.calledSuit(), game.taker());
        const bool isCalled = game.seatCount() == 5 && game.callee() >= 0 &&
                              played.id == calledNow.id;
        if (isCalled) {
            calledCardSeen = true;
            revealedAfterCalledCard = game.callRevealed();
            if (wasRevealed) revealedBeforeCalledCard = true;  // déjà révélé avant : anormal
        } else if (game.callRevealed() && !wasRevealed) {
            revealedBeforeCalledCard = true;  // révélé sans que la carte appelée soit jouée
        }
    }
    return callee;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    int failures = 0;

    // 1. Appel caché : pas de révélation avant la carte appelée, révélation
    //    quand elle est posée, message sans le nom du partenaire.
    for (int trial = 0; trial < 6; ++trial) {
        Tarot game;
        game.startMatch(specsFor(5), 8, true, true);
        QVector<QPair<bool, QString>> msgs;
        bool early = false, after = false, seen = false;
        const int callee = playDonneHidden(game, msgs, early, after, seen);
        if (callee == -2 || callee == -3 || callee == -4 || callee == -5 ||
            callee == -6 || callee == -7 || callee == -8) {
            std::printf("donne a échoué (code %d, phase=%d)\n", callee, int(game.phase()));
            ++failures;
            continue;
        }
        if (callee >= 0) {
            if (!seen) { std::printf("  ECHEC carte appelée jamais jouée ?\n"); ++failures; }
            if (early) { std::printf("  ECHEC révélé avant la carte appelée\n"); ++failures; }
            if (!after) { std::printf("  ECHEC pas révélé après la carte appelée\n"); ++failures; }
            // Le message d'appel ne doit pas contenir le nom de l'appelé.
            const QString calleeName = game.seat(callee).name;
            bool named = false;
            for (const auto &entry : msgs) {
                const QString &m = entry.second;
                if (!entry.first && m.contains(calleeName) && m.contains(QStringLiteral("partenaire")))
                    named = true;
            }
            if (named) { std::printf("  ECHEC le message nomme l'appelé (%s) avant la révélation\n", qPrintable(calleeName)); ++failures; }
            // La donne finie, la révélation a eu lieu.
            if (!game.callRevealed()) { std::printf("  ECHEC callRevealed faux en fin de donne\n"); ++failures; }
            // Donne suivante : callRevealed retombe à faux.
            game.startDonne();
            if (game.callRevealed()) { std::printf("  ECHEC callRevealed pas remis à zéro\n"); ++failures; }
            std::printf("caché, donne %d : appelé=%d (%s), révélé seulement par la carte jouée\n",
                        trial, callee, qPrintable(calleeName));
        } else {
            // Carte appelée chez le preneur ou au chien : seul contre 4, pas
            // de révélation possible.
            std::printf("caché, donne %d : seul contre 4\n", trial);
            if (game.callRevealed()) { std::printf("  ECHEC révélé sans appelé\n"); ++failures; }
        }
    }

    // 2. La révélation est annoncée quand la carte appelée est posée.
    {
        Tarot game;
        game.startMatch(specsFor(5), 8, true, true);
        QVector<QPair<bool, QString>> msgs;
        bool early = false, after = false, seen = false;
        const int callee = playDonneHidden(game, msgs, early, after, seen);
        if (callee < 0 && callee != -1) { std::printf("donne a échoué (code %d)\n", callee); ++failures; }
        else if (callee >= 0) {
            const QString calleeName = game.seat(callee).name;
            bool announced = false;
            for (const auto &entry : msgs) {
                const QString &m = entry.second;
                if (entry.first && m.contains(QStringLiteral("carte appelée")) && m.contains(calleeName))
                    announced = true;
            }
            if (!announced) { std::printf("  ECHEC révélation non annoncée (appelé=%d %s)\n", callee, qPrintable(calleeName)); ++failures; }
            else std::printf("révélation annoncée: appelé=%d (%s) nommé au moment de la carte appelée\n", callee, qPrintable(calleeName));
        }
    }

    // 3. À 4 joueurs : rien ne change, pas de révélation, pas d'appel.
    {
        Tarot game;
        game.startMatch(specsFor(4), 1, true, true);
        QStringList msgs;
        int guard = 0;
        while (game.phase() == Phase::Bidding && guard++ < 20) {
            if (game.turn() == 0) game.makeBid(Contract::Petite); else game.pass();
        }
        if (game.phase() == Phase::Appel) { std::printf("  ECHEC phase Appel à 4 joueurs\n"); ++failures; }
        if (game.callee() != -1) { std::printf("  ECHEC callee=%d à 4 joueurs\n", game.callee()); ++failures; }
        if (game.callRevealed()) { std::printf("  ECHEC callRevealed à 4 joueurs\n"); ++failures; }
        std::printf("4 sièges: pas d'appel, callRevealed faux\n");
    }

    std::printf(failures == 0 ? "TOUT OK\n" : "%d ECHEC\n", failures);
    return failures == 0 ? 0 : 1;
}
