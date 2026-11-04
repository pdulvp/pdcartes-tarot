// Le modèle joue à 3, 4 et 5 joueurs — distribution selon le
// règlement FFT, donnes jouées jusqu'au bout (appel du roi compris à 5),
// marque de somme nulle.
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

// Joue une donne entière, au hasard des cartes légales.
static bool playDonne(Tarot &game, int takerWanted) {
    int guard = 0;
    while (game.phase() == Phase::Bidding && guard++ < 20) {
        QString why;
        if (game.turn() == takerWanted) {
            if (!game.makeBid(Contract::Petite, &why)) return false;
        } else if (!game.pass(&why)) {
            return false;
        }
    }
    if (game.phase() == Phase::Idle) return false;  // donne annulée : on en refait une
    if (game.taker() != takerWanted) return false;
    // À 5 joueurs, le preneur appelle un roi avant de voir le chien.
    if (game.phase() == Phase::Appel) {
        QString why;
        Suit suit = Suit::Pique;
        for (int s = 0; s < 4; ++s) {
            if (!findCard(game.seat(takerWanted).hand,
                          game.calledCardFor(Suit(s), takerWanted).id)) {
                suit = Suit(s);
                break;
            }
        }
        if (!game.callKing(takerWanted, suit, &why)) return false;
    }
    if (game.phase() == Phase::Chien) game.chienSeen();
    if (game.phase() == Phase::Ecart) {
        if (game.ecartNeeded()) {
            QVector<int> ids;
            for (const Card &c : game.seat(game.taker()).hand) {
                if (game.mayDiscard(c)) ids.append(c.id);
                if (ids.size() >= game.chienSize()) break;
            }
            QString why;
            if (!game.setEcart(ids, &why)) return false;
        }
        game.finishEcart();
    }
    guard = 0;
    while (game.phase() == Phase::Playing || game.phase() == Phase::TrickOver) {
        if (++guard > 500) return false;
        if (game.phase() == Phase::TrickOver) {
            game.sweepTrick();
            continue;
        }
        const int at = game.turn();
        const QVector<int> ok = game.legalCards(at);
        if (ok.isEmpty()) return false;
        QString why;
        if (!game.play(ok.first(), &why)) {
            std::printf("  play refusé siège %d : %s\n", at, qPrintable(why));
            return false;
        }
    }
    return game.phase() == Phase::DonneOver || game.phase() == Phase::MatchOver;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    int failures = 0;
    for (const int seats : {3, 4, 5}) {
        Tarot game;
        game.startMatch(specsFor(seats), 1, true, true);
        if (game.seatCount() != seats) { std::printf("%d sièges: ECHEC seatCount=%d\n", seats, game.seatCount()); ++failures; continue; }
        // Joue jusqu'à trois donnes (certaines peuvent être annulées).
        int donnes = 0;
        for (int tries = 0; tries < 6 && donnes < 2; ++tries) {
            if (!playDonne(game, 0)) {
                if (game.phase() == Phase::Idle || game.phase() == Phase::MatchOver) break;
                std::printf("%d sièges: donne %d a échoué (phase=%d)\n", seats, tries, int(game.phase()));
                ++failures;
                break;
            }
            ++donnes;
            const DonneResult &done = game.result();
            int sum = 0;
            for (int at = 0; at < seats; ++at) {
                const int hs = game.seat(at).hand.size();
                if (at == 0 && hs != 0) { }  // main vidée après la donne
                sum += done.gained[at];
            }
            const int totalCards = seats * game.handSize() + game.chienSize();
            std::printf("%d sièges: donne jouée, contrat=%d preneur=%d plis=%d cartes=%d/%d gained sum=%d made=%d\n",
                        seats, int(done.contract), done.taker, done.tricks[0] + done.tricks[1],
                        totalCards, 78, sum, int(done.made));
            if (totalCards != 78) { std::printf("  ECHEC total cartes\n"); ++failures; }
            if (done.tricks[0] + done.tricks[1] != game.trickCount()) { std::printf("  ECHEC plis\n"); ++failures; }
            if (sum != 0) { std::printf("  ECHEC somme des gains\n"); ++failures; }
            if (game.phase() == Phase::MatchOver) break;
            game.startDonne();
        }
        // Tailles de main au moment de la distribution : on redonne une donne
        // fraîche et on compte avant enchères.
        Tarot check;
        check.startMatch(specsFor(seats), 1, true, true);
        int hands = 0;
        for (int at = 0; at < seats; ++at) hands += check.seat(at).hand.size();
        const int chien = check.chienSize();
        std::printf("%d sièges: main=%d chien=%d total=%d (attendu main=%d chien=%d)\n",
                    seats, hands, chien, hands + chien, seats * check.handSize(),
                    seats == 5 ? 3 : 6);
        if (hands != seats * check.handSize() || chien != (seats == 5 ? 3 : 6) ||
            hands + chien != 78) {
            ++failures;
        }
    }
    std::printf(failures == 0 ? "TOUT OK\n" : "%d ECHEC\n", failures);
    return failures == 0 ? 0 : 1;
}
