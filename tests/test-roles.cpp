// Plan #2 (pdcartes-tarot) : la pastille de rôle des plaques — « preneur »,
// Les pastilles de rôle des plaques : « avec vous », « contre vous », « preneur »,
// selon le camp vu de vous, au tarot à quatre
// où le preneur est seul contre les trois autres.
#include "Tarot.h"

#include <QCoreApplication>
#include <cstdio>

// L'expression exacte de TableView::paintSeatPlaque().
static QString roleOf(const Tarot &game, int at, int me) {
    const int takerSeat = game.taker();
    const bool taker = takerSeat == at;
    return takerSeat < 0          ? QStringLiteral("à l'enchère")
           : taker                ? QStringLiteral("preneur")
           : takerSeat == me      ? QStringLiteral("contre vous")
                                  : QStringLiteral("avec vous");
}

static void makeGame(Tarot &game, int takerSeatWanted) {
    // le jeu est passé en paramètre
    QVector<SeatSpec> specs;
    specs.append({QString(), QStringLiteral("Vous"), 0});
    specs.append({QStringLiteral("r1"), QStringLiteral("Alice"), 40});
    specs.append({QStringLiteral("r2"), QStringLiteral("Bob"), 120});
    specs.append({QStringLiteral("r3"), QStringLiteral("Chloe"), 200});
    game.startMatch(specs, 3, false, false);
    // L'enchère commence à la place du donneur + 1 ; le donneur est la
    // dernière place, donc c'est vous (place 0) qui parlez la première.
    int guard = 0;
    while (game.phase() == Phase::Bidding && guard++ < 10) {
        QString why;
        if (game.turn() == takerSeatWanted) {
            if (!game.makeBid(Contract::Petite, &why)) std::printf("bid refusée: %s\n", qPrintable(why));
        } else {
            if (!game.pass(&why)) std::printf("passe refusé: %s\n", qPrintable(why));
        }
    }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    int me = -1;
    int failures = 0;

    // Vous prenez : les trois autres doivent être « contre vous ».
    {
        Tarot game; makeGame(game, 0);
        me = -1;
        for (int at = 0; at < game.seatCount(); ++at)
            if (game.seat(at).human()) me = at;
        std::printf("vous prenez (taker=%d, mon siège=%d):\n", game.taker(), me);
        for (int at = 0; at < game.seatCount(); ++at) {
            if (at == me) continue;
            const QString role = roleOf(game, at, me);
            const bool ok = role == QStringLiteral("contre vous") &&
                            game.teamOf(at) == kDefenceTeam;
            std::printf("  siège %d (%s): %s [%s]\n", at, qPrintable(game.seat(at).name),
                        qPrintable(role), ok ? "OK" : "ECHEC");
            if (!ok) ++failures;
        }
        if (game.taker() != 0) { std::printf("  ECHEC taker=%d\n", game.taker()); ++failures; }
    }

    // Un autre prend : lui « preneur », les deux autres « avec vous ».
    {
        Tarot game; makeGame(game, 1);
        me = -1;
        for (int at = 0; at < game.seatCount(); ++at)
            if (game.seat(at).human()) me = at;
        std::printf("un autre prend (taker=%d, mon siège=%d):\n", game.taker(), me);
        for (int at = 0; at < game.seatCount(); ++at) {
            if (at == me) continue;
            const QString role = roleOf(game, at, me);
            const bool taker = game.taker() == at;
            const bool ok = taker ? (role == QStringLiteral("preneur") && game.teamOf(at) == kTakerTeam)
                                  : (role == QStringLiteral("avec vous") &&
                                     game.teamOf(at) == kDefenceTeam);
            std::printf("  siège %d (%s): %s [%s]\n", at, qPrintable(game.seat(at).name),
                        qPrintable(role), ok ? "OK" : "ECHEC");
            if (!ok) ++failures;
        }
        if (game.taker() != 1) { std::printf("  ECHEC taker=%d\n", game.taker()); ++failures; }
    }

    std::printf(failures == 0 ? "TOUT OK\n" : "%d ECHEC\n", failures);
    return failures == 0 ? 0 : 1;
}
