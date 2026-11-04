// Combien de fois les IA surenchérissent-elles après une petite du joueur ?
#include "Tarot.h"
#include "Brain.h"

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

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const int seats = argc > 1 ? atoi(argv[1]) : 4;
    int donnes = 0, aiBids = 0, aiPasses = 0, takerIsAi = 0;
    for (int d = 0; d < 30; ++d) {
        Tarot game;
        game.startMatch(specsFor(seats), 1, true, true);
        int guard = 0;
        while (game.phase() == Phase::Bidding && guard++ < 20) {
            const int at = game.turn();
            if (game.seat(at).human()) {
                game.makeBid(Contract::Petite);   // vous prenez la petite
            } else {
                const Contract wanted = chooseBid(game, at, bestSkill());
                if (wanted != Contract::Passe && game.makeBid(wanted)) {
                    ++aiBids;
                } else {
                    ++aiPasses;
                    game.pass();
                }
            }
        }
        if (game.taker() >= 0) {
            ++donnes;
            if (game.taker() != 0) ++takerIsAi;
        }
    }
    std::printf("%d joueurs, %d donnes: IA ont surenchéri %d fois, passé %d fois ; "
                "preneur = une IA dans %d donnes\n",
                seats, donnes, aiBids, aiPasses, takerIsAi);
    return 0;
}
