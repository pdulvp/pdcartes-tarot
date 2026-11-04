// À 5 joueurs, l'appel du roi : phase, camps, restrictions d'écart et
// d'entame, marque 2/1/-1x3 (somme nulle) ; seul contre 4 quand la carte
// appelée est au chien ou chez le preneur ; rien ne change à 3/4 joueurs.
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

static bool playDonne(Tarot &game, int takerWanted, bool humanCalls) {
    int guard = 0;
    while (game.phase() == Phase::Bidding && guard++ < 20) {
        QString why;
        if (game.turn() == takerWanted) {
            if (!game.makeBid(Contract::Petite, &why)) return false;
        } else if (!game.pass(&why)) {
            return false;
        }
    }
    if (game.taker() != takerWanted) return false;
    if (game.seatCount() == 5) {
        if (game.phase() != Phase::Appel) return false;
        if (!game.mayCall(takerWanted)) return false;
        // Appelle la première couleur dont la carte appelée n'est pas en main.
        Suit suit = Suit::Pique;
        for (int s = 0; s < 4; ++s) {
            if (!findCard(game.seat(takerWanted).hand, game.calledCardFor(Suit(s), takerWanted).id)) {
                suit = Suit(s);
                break;
            }
        }
        QString why;
        if (!game.callKing(takerWanted, suit, &why)) {
            std::printf("  appel refusé : %s\n", qPrintable(why));
            return false;
        }
        (void)humanCalls;
    } else if (game.phase() == Phase::Appel) {
        return false;  // pas de phase Appel à 3 ou 4 joueurs
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
            if (!game.setEcart(ids, &why)) {
                std::printf("  écart refusé : %s\n", qPrintable(why));
                return false;
            }
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
        if (!game.play(ok.first())) return false;
    }
    return game.phase() == Phase::DonneOver || game.phase() == Phase::MatchOver;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    int failures = 0;

    // À 5 joueurs, plusieurs donnes : vérifier camps, restrictions, marque.
    {
        Tarot game;
        game.startMatch(specsFor(5), 4, true, true);
        int withCallee = 0, alone = 0;
        for (int d = 0; d < 4; ++d) {
            if (d > 0) game.startDonne();
            if (!playDonne(game, 0, true)) { std::printf("5 sièges: donne %d a échoué (phase=%d)\n", d, int(game.phase())); ++failures; break; }
            const DonneResult &done = game.result();
            const int callee = done.callee;
            int sum = 0;
            for (int at = 0; at < 5; ++at) sum += done.gained[at];
            if (sum != 0) { std::printf("  ECHEC somme=%d\n", sum); ++failures; }
            if (callee >= 0) {
                ++withCallee;
                // camps : preneur + appelé ensemble
                if (game.teamOf(done.taker) != game.teamOf(callee)) { std::printf("  ECHEC camps\n"); ++failures; }
                if (done.gained[done.taker] != 2 * done.gained[callee] ||
                    done.gained[callee] != -done.gained[(done.taker + 1) % 5 == callee ? (done.taker + 2) % 5 : (done.taker + 1) % 5]) {
                    std::printf("  ECHEC répartition 2/1/-1 (taker=%d callee=%d gained=%d,%d)\n",
                                done.gained[done.taker], done.gained[callee], done.gained[done.taker], done.gained[callee]);
                    ++failures;
                }
            } else {
                ++alone;
                if (done.gained[done.taker] != -4 * done.gained[(done.taker + 1) % 5]) {
                    std::printf("  ECHEC répartition 4/-1 (taker=%d)\n", done.gained[done.taker]);
                    ++failures;
                }
            }
            if (game.phase() == Phase::MatchOver) break;
        }
        std::printf("5 sièges: %d donnes avec appelé, %d seul — sommes nulles\n", withCallee, alone);
    }

    // Restrictions à 5 joueurs : écart et entame.
    {
        Tarot game;
        game.startMatch(specsFor(5), 1, true, true);
        int guard = 0;
        while (game.phase() == Phase::Bidding && guard++ < 20) {
            if (game.turn() == 0) game.makeBid(Contract::Petite); else game.pass();
        }
        // Appelle pique tant que la carte appelée n'est pas chez le preneur.
        Suit suit = Suit::Pique;
        for (int s = 0; s < 4; ++s) {
            if (!findCard(game.seat(0).hand, game.calledCardFor(Suit(s), 0).id)) { suit = Suit(s); break; }
        }
        game.callKing(0, suit);
        const int callee = game.callee();
        if (callee >= 0) {
            // Écart : aucune carte de la couleur appelée ne peut être écartée
            // tant qu'il reste assez d'autres cartes.
            game.chienSeen();
            int freeOthers = 0;
            for (const Card &c : game.seat(0).hand) {
                if (c.suit != game.calledSuit() && game.mayDiscard(c)) ++freeOthers;
            }
            if (freeOthers >= game.chienSize()) {
                for (const Card &c : game.seat(0).hand) {
                    if (c.suit == game.calledSuit() && game.mayDiscard(c)) {
                        std::printf("  ECHEC écart couleur appelée autorisée\n");
                        ++failures;
                        break;
                    }
                }
            }
            // Écart valide puis entame : la couleur appelée est interdite au
            // premier pli sauf la carte appelée.
            QVector<int> ids;
            for (const Card &c : game.seat(0).hand) {
                if (game.mayDiscard(c)) ids.append(c.id);
                if (ids.size() >= game.chienSize()) break;
            }
            game.setEcart(ids);
            game.finishEcart();
            const int leader = game.turn();
            const QVector<int> ok = game.legalCards(leader);
            for (const Card &c : game.seat(leader).hand) {
                if (c.suit == game.calledSuit() && !ok.contains(c.id)) {
                    // interdit à l'entame : bon
                } else if (c.suit == game.calledSuit() && ok.contains(c.id)) {
                    // autorisé seulement si c'est la carte appelée elle-même
                    const Card called = game.calledCardFor(game.calledSuit(), game.taker());
                    if (c.id != called.id) {
                        std::printf("  ECHEC entame couleur appelée autorisée\n");
                        ++failures;
                    }
                }
            }
            std::printf("appel: couleur=%d appelé=%d, entame restreinte vérifiée\n",
                        int(game.calledSuit()), callee);
        } else {
            std::printf("appel: carte appelée chez le preneur ou au chien -> seul contre 4 (callee=%d)\n", callee);
        }
    }

    // À 3 et 4 joueurs : pas de phase Appel, rien ne change.
    for (const int seats : {3, 4}) {
        Tarot game;
        game.startMatch(specsFor(seats), 1, true, true);
        if (!playDonne(game, 0, false)) { std::printf("%d sièges: donne a échoué\n", seats); ++failures; }
        else if (game.result().callee != -1) { std::printf("%d sièges: ECHEC callee=%d\n", seats, game.result().callee); ++failures; }
        else std::printf("%d sièges: donne ok, pas d'appel (callee=-1)\n", seats);
    }

    std::printf(failures == 0 ? "TOUT OK\n" : "%d ECHEC\n", failures);
    return failures == 0 ? 0 : 1;
}
