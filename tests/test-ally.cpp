// À 5 joueurs, l'appelé joue-t-il vraiment en allié ?
// Le joueur (siège 0) prend toujours, appelle un roi, et entame le premier
// pli avec sa plus forte carte. On regarde ensuite ce que joue chaque IA
// quand le camp du preneur tient le pli : l'appelé doit charger (gros points),
// un défenseur non.
#include "Tarot.h"
#include "Brain.h"

#include <QCoreApplication>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

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
    const int donnes = argc > 1 ? atoi(argv[1]) : 150;
    int calleeCharges = 0, calleeCases = 0;      // appelé, dernier à parler, pli à nous
    int defDumps = 0, defCases = 0;              // défenseur, dernier à parler, pli au preneur
    int teamOfOk = 0, teamOfBad = 0;
    int calleeFound = 0;

    for (int d = 0; d < donnes; ++d) {
        Tarot game;
        game.startMatch(specsFor(5), 1, true, true);
        int guard = 0;
        while (game.phase() == Phase::Bidding && guard++ < 20) {
            if (game.turn() == 0) game.makeBid(Contract::Petite);
            else game.pass();
        }
        if (game.taker() != 0) continue;
        // Appelle la première couleur dont la carte appelée n'est pas en main.
        Suit suit = Suit::Pique;
        for (int s = 0; s < 4; ++s) {
            if (!findCard(game.seat(0).hand, game.calledCardFor(Suit(s), 0).id)) { suit = Suit(s); break; }
        }
        game.callKing(0, suit);
        const int callee = game.callee();
        if (callee >= 0) {
            ++calleeFound;
            if (game.teamOf(callee) == game.teamOf(game.taker()) &&
                game.teamOf(callee) == kTakerTeam)
                ++teamOfOk;
            else
                ++teamOfBad;
        }
        if (game.phase() == Phase::Chien) game.chienSeen();
        if (game.phase() == Phase::Ecart) {
            QVector<int> ids;
            for (const Card &c : game.seat(0).hand) {
                if (game.mayDiscard(c)) ids.append(c.id);
                if (ids.size() >= game.chienSize()) break;
            }
            game.setEcart(ids);
            game.finishEcart();
        }
        if (game.phase() != Phase::Playing) continue;

        // Premier pli : le joueur (donneur+1) entame avec sa plus forte carte.
        const QVector<Card> &hand0 = game.seat(0).hand;
        Card best;
        for (const Card &c : hand0) {
            if (!best.valid() || cardStrength(c) > cardStrength(best)) best = c;
        }
        if (!game.play(best.id)) continue;
        // Les autres jouent dans l'ordre des sièges.
        for (int step = 1; step < 5; ++step) {
            const int at = game.turn();
            if (at == 0) break;
            const int master = game.trickMaster();
            const bool teamHolds = master >= 0 && game.teamOf(master) == game.teamOf(game.taker());
            const bool last = game.trick().size() == game.seatCount() - 1;
            const int card = chooseCard(game, at, bestSkill());
            const Card *held = findCard(game.seat(at).hand, card);
            const int pts = held ? cardPoints(*held) : 0;
            // points max et min parmi les cartes légales jouables : hors
            // excuse, et hors petit pour le min (un expert garde son petit).
            int maxPts = 0, minPts = 99;
            Card top;
            if (master >= 0)
                for (const Played &p : game.trick())
                    if (p.seat == master) top = p.card;
            Card cheapestWinner;
            for (int id : game.legalCards(at)) {
                const Card *c = findCard(game.seat(at).hand, id);
                if (!c || c->excuse()) continue;
                maxPts = std::max(maxPts, cardPoints(*c));
                if (c->suit == Suit::Atout && c->rank == kPetit) continue;
                minPts = std::min(minPts, cardPoints(*c));
                // s'il peut prendre, le défenseur prend au plus juste : la
                // plus faible des cartes qui battent le pli
                if (master >= 0 && beatsTrick(*c, top, game.ledSuit()) &&
                    (!cheapestWinner.valid() || cardStrength(*c) < cardStrength(cheapestWinner)))
                    cheapestWinner = *c;
            }
            if (teamHolds && last) {
                if (at == callee || master == callee) {
                    // L'appelé charge le pli du preneur — et un défenseur
                    // charge aussi le pli de l'appelé : l'appel étant caché,
                    // il lui paraît un défenseur comme lui. Dans les deux cas
                    // c'est la carte la plus chère (le petit compris : un pli
                    // acquis à son camp est le seul endroit où il est sûr).
                    ++calleeCases;
                    if (pts == maxPts) ++calleeCharges;
                } else {
                    ++defCases;
                    // se débarrasser de la moins chère, prendre au plus juste,
                    // ou poser l'excuse plutôt qu'une carte qui coûte cher
                    if (pts == minPts || (held && held->excuse()) ||
                        (held && cheapestWinner.valid() && held->id == cheapestWinner.id))
                        ++defDumps;
                }
            }
            if (!game.play(card)) break;
        }
        game.sweepTrick();
    }
    std::printf("donnes=%d appelés=%d teamOf ok=%d bad=%d\n", donnes, calleeFound, teamOfOk, teamOfBad);
    std::printf("dernier à parler, pli acquis au camp du preneur :\n");
    std::printf("  camp preneur (appelé + défenseur voyant l'appelé) : %d cas, a chargé %d fois (%.0f%%)\n",
                calleeCases, calleeCharges, calleeCases ? 100.0 * calleeCharges / calleeCases : 0.0);
    std::printf("  défenseur : %d cas, s'est débarrassé (carte la moins chère) %d fois (%.0f%%)\n",
                defCases, defDumps, defCases ? 100.0 * defDumps / defCases : 0.0);
    // Capacités selon le skillset (#13) : débutant contre tout au maximum.
    Learning l0;
    const Skill beginner = skillFor(l0, Innates{});
    Learning lMax;
    lMax.xp = kMaxExperience;
    for (int i = 0; i < kLearnedCount; ++i) lMax.pools.xp[i] = kPoolCeiling;
    const Skill expert = skillFor(lMax, Innates{});
    std::printf("skillset débutant : blunder=%.2f readsTable=%d | max : blunder=%.2f readsTable=%d\n",
                beginner.blunder, int(beginner.readsTable), expert.blunder, int(expert.readsTable));
    const bool ok = teamOfBad == 0 && calleeCases >= 1 && defCases >= 1 &&
                    calleeCharges == calleeCases && defDumps == defCases &&
                    beginner.blunder > expert.blunder && expert.readsTable && !beginner.readsTable;
    std::printf("%s\n", ok ? "TOUT OK" : "ECHEC");
    return ok ? 0 : 1;
}
