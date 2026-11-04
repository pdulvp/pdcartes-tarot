// Le cerveau respecte l'appel caché (#11) : knownTeamOf ne dit pas l'appelé
// avant la révélation (sauf à l'appelé lui-même), guessedCallee devine d'après
// les points donnés au preneur, et chooseCard suit : l'appelé charge les plis
// du preneur, un défenseur ne charge le pli de l'appelé caché que tant que la
// devinette ne le désigne pas.
#include "Brain.h"
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

// La plus chère des cartes légales (hors excuse), comme charge le cerveau :
// ex æquo, c'est la plus faible en force qui gagne (cf. dearest de Brain.cpp).
static Card dearestLegal(const Tarot &game, int at) {
    Card best;
    for (int id : game.legalCards(at)) {
        const Card *c = findCard(game.seat(at).hand, id);
        if (!c || c->excuse()) continue;
        if (!best.valid() || cardPoints(*c) > cardPoints(best) ||
            (cardPoints(*c) == cardPoints(best) && cardStrength(*c) < cardStrength(best)))
            best = *c;
    }
    return best;
}

// Cartes légales qui battent le pli courant, et la plus faible d'entre elles
// (celle que joue « au plus juste » un défenseur qui prend).
static QVector<Card> winnersOf(const Tarot &game, int at, Card *cheapestWinner) {
    QVector<Card> winners;
    const int master = game.trickMaster();
    if (master >= 0) {
        Card top;
        for (const Played &p : game.trick())
            if (p.seat == master) top = p.card;
        for (int id : game.legalCards(at)) {
            const Card *c = findCard(game.seat(at).hand, id);
            if (c && !c->excuse() && beatsTrick(*c, top, game.ledSuit())) winners.append(*c);
        }
    }
    if (cheapestWinner) {
        *cheapestWinner = Card{};
        for (const Card &c : winners)
            if (!cheapestWinner->valid() || cardStrength(c) < cardStrength(*cheapestWinner))
                *cheapestWinner = c;
    }
    return winners;
}

struct Counters {
    bool hidBefore = true;      // knownTeamOf cache l'appelé avant révélation
    bool shownAfter = true;     // ... et le montre après
    bool guessOk = true;        // guessedCallee == soupçon recalculé
    bool adviseLeak = false;    // le conseil ne dit pas « votre camp » pour l'appelé caché
    int calleeCases = 0, calleeCharges = 0;
    int defCases = 0, defCharges = 0;       // pli de l'appelé caché, devinette ailleurs : on charge
    int guessCases = 0, guessCharges = 0;   // devinette == appelé : on ne charge plus
};

// Joue une donne à 5 sièges, siège 0 preneur, tout le monde au cerveau
// expert. Renvoie le siège de l'appelé (-1 si carte appelée au chien).
static int playDonne(Tarot &game, Counters &c) {
    int guard = 0;
    while (game.phase() == Phase::Bidding && guard++ < 20) {
        if (game.turn() == 0) game.makeBid(Contract::Petite);
        else game.pass();
    }
    if (game.taker() != 0 || game.phase() != Phase::Appel) return -2;
    Suit suit = Suit::Pique;
    for (int s = 0; s < 4; ++s) {
        if (!findCard(game.seat(0).hand, game.calledCardFor(Suit(s), 0).id)) { suit = Suit(s); break; }
    }
    if (!game.callKing(0, suit)) return -2;
    const int callee = game.callee();
    if (game.phase() == Phase::Chien) game.chienSeen();
    if (game.phase() == Phase::Ecart) {
        if (game.ecartNeeded()) {
            QVector<int> ids;
            for (const Card &card : game.seat(0).hand) {
                if (game.mayDiscard(card)) ids.append(card.id);
                if (ids.size() >= game.chienSize()) break;
            }
            if (!game.setEcart(ids)) return -2;
        }
        game.finishEcart();
    }
    if (callee >= 0 && game.guessedCallee() != -1) c.guessOk = false;   // aucun pli joué

    const Skill expert = bestSkill();
    guard = 0;
    while (game.phase() == Phase::Playing || game.phase() == Phase::TrickOver) {
        if (++guard > 500) return -3;
        if (game.phase() == Phase::TrickOver) { game.sweepTrick(); continue; }
        const int at = game.turn();
        const int master = game.trickMaster();
        const bool last = game.trick().size() == game.seatCount() - 1;
        const bool revealed = game.callRevealed();

        if (callee >= 0 && !revealed) {
            for (int s = 0; s < game.seatCount(); ++s) {
                if (s == callee) {
                    if (game.knownTeamOf(callee, callee) != kTakerTeam) c.hidBefore = false;
                } else if (game.knownTeamOf(callee, s) != kDefenceTeam) {
                    c.hidBefore = false;
                }
                if (game.knownTeamOf(0, s) != kTakerTeam) c.hidBefore = false;
            }
            // Devinette == soupçon recalculé sur l'historique public.
            QVector<int> suspicion(game.seatCount(), 0);
            const int chunk = game.seatCount();
            const QVector<Played> &hist = game.history();
            for (int start = 0; start + chunk <= hist.size(); start += chunk) {
                Suit led = Suit::Pique;
                for (int i = start; i < start + chunk; ++i)
                    if (!hist.at(i).card.excuse()) { led = hist.at(i).card.suit; break; }
                int winner = -1;
                Card top;
                for (int i = start; i < start + chunk; ++i) {
                    if (!beatsTrick(hist.at(i).card, top, led)) continue;
                    top = hist.at(i).card;
                    winner = hist.at(i).seat;
                }
                if (winner != 0) continue;
                for (int i = start; i < start + chunk; ++i)
                    if (hist.at(i).seat != 0) suspicion[hist.at(i).seat] += cardPoints(hist.at(i).card);
            }
            int expect = -1;
            for (int s = 0; s < game.seatCount(); ++s) {
                if (s == 0 || suspicion[s] <= 0) continue;
                if (expect < 0 || suspicion[s] > suspicion[expect]) expect = s;
            }
            if (game.guessedCallee() != expect) c.guessOk = false;
            // Le conseil ne doit pas éventer l'appelé caché au preneur : tant
            // que la devinette ne désigne pas l'appelé, un pli tenu par lui
            // n'est pas « au camp » du preneur. (Un défenseur, lui, voit
            // l'appelé caché comme un défenseur de plus — c'est la
            // connaissance publique, et c'est voulu.)
            if (master == callee && at == 0 && game.guessedCallee() != callee &&
                advise(game, at).contains(QStringLiteral("votre camp")))
                c.adviseLeak = true;
        }

        // Comportement au dernier rang.
        const bool watched = callee >= 0 && !revealed && last;
        const Card dearestBefore = watched ? dearestLegal(game, at) : Card{};
        Card cheapestWin;
        const QVector<Card> winnersBefore = watched ? winnersOf(game, at, &cheapestWin) : QVector<Card>{};
        const int legalCount = game.legalCards(at).size();
        const int guess = game.guessedCallee();

        const int cardId = chooseCard(game, at, expert);
        const bool wasRevealed = game.callRevealed();
        if (cardId < 0 || !game.play(cardId)) return -3;

        if (watched && legalCount > 1 && dearestBefore.valid()) {
            if (at == callee && master == 0) {
                ++c.calleeCases;
                if (cardId == dearestBefore.id) ++c.calleeCharges;
            }
            if (at != 0 && at != callee && master == callee) {
                // Tant que la devinette ne désigne pas l'appelé, le défenseur
                // expert le compte défenseur comme tout le monde : il CHARGE
                // son pli (c'est la connaissance publique). Dès que la
                // devinette le désigne, il ne charge plus et essaie de prendre.
                if (guess != callee) {
                    ++c.defCases;
                    if (cardId == dearestBefore.id) ++c.defCharges;
                } else {
                    ++c.guessCases;
                    // charge seulement si la dearest n'est pas aussi la plus
                    // faible des gagnantes (prendre au plus juste, c'est normal)
                    if (winnersBefore.size() >= 2 && cardId == dearestBefore.id &&
                        cardId != cheapestWin.id)
                        ++c.guessCharges;
                }
            }
        }
        if (callee >= 0 && game.callRevealed() && !wasRevealed) {
            for (int s = 0; s < game.seatCount(); ++s)
                if (game.knownTeamOf(callee, s) != kTakerTeam) c.shownAfter = false;
            if (game.guessedCallee() != callee) c.guessOk = false;
        }
    }
    return callee;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    int failures = 0;

    Counters c;
    int donnesWithCallee = 0;
    for (int trial = 0; trial < 8; ++trial) {
        Tarot game;
        game.startMatch(specsFor(5), 40, true, true);
        for (int d = 0; d < 5; ++d) {
            if (d > 0) game.startDonne();
            const int callee = playDonne(game, c);
            if (callee == -2 || callee == -3) { std::printf("donne bloquée (code %d)\n", callee); ++failures; break; }
            if (callee >= 0) ++donnesWithCallee;
            if (game.phase() == Phase::MatchOver) break;
        }
    }
    if (!c.hidBefore) { std::printf("ECHEC knownTeamOf dit l'appelé avant révélation\n"); ++failures; }
    if (!c.shownAfter) { std::printf("ECHEC knownTeamOf ne montre pas l'appelé après révélation\n"); ++failures; }
    if (!c.guessOk) { std::printf("ECHEC guessedCallee != soupçon recalculé\n"); ++failures; }
    if (c.adviseLeak) { std::printf("ECHEC le conseil dit 'votre camp' pour un pli à l'appelé caché\n"); ++failures; }
    if (c.calleeCases > 0 && c.calleeCharges != c.calleeCases) {
        std::printf("ECHEC appelé expert n'a pas chargé %d/%d plis du preneur\n",
                    c.calleeCharges, c.calleeCases);
        ++failures;
    }
    if (c.defCases > 0 && c.defCharges != c.defCases) {
        std::printf("ECHEC défenseur expert n'a pas chargé %d/%d plis de l'appelé caché (devinette ailleurs)\n",
                    c.defCharges, c.defCases);
        ++failures;
    }
    if (c.guessCharges > 0) {
        std::printf("ECHEC défenseur expert a chargé %d/%d plis de l'appelé deviné\n",
                    c.guessCharges, c.guessCases);
        ++failures;
    }
    std::printf("caché: %d donnes avec appelé — camps cachés/montrés ok, devinette conforme, conseil sans fuite\n",
                donnesWithCallee);
    std::printf("  appelé expert, dernier sur pli du preneur : %d cas, %d charges\n", c.calleeCases, c.calleeCharges);
    std::printf("  défenseur expert, dernier sur pli de l'appelé caché, devinette ailleurs : %d cas, %d charges (attendu: tous)\n",
                c.defCases, c.defCharges);
    std::printf("  défenseur expert, dernier sur pli de l'appelé DEVINÉ : %d cas, %d charges (attendu: 0)\n",
                c.guessCases, c.guessCharges);

    // Après la révélation (fin de donne), knownTeamOf == teamOf partout.
    {
        Tarot game;
        game.startMatch(specsFor(5), 4, true, true);
        Counters v;
        const int callee = playDonne(game, v);
        if (callee >= 0 && game.callRevealed()) {
            for (int s = 0; s < 5; ++s)
                if (game.knownTeamOf(callee, s) != game.teamOf(callee)) {
                    std::printf("ECHEC après révélation: knownTeamOf != teamOf\n");
                    ++failures;
                    break;
                }
            std::printf("après révélation: appelé=%d, knownTeamOf == teamOf partout\n", callee);
        } else {
            std::printf("donne sans appelé (chien)\n");
        }
    }

    // 4 sièges : rien ne change.
    {
        Tarot game;
        game.startMatch(specsFor(4), 1, true, true);
        if (game.guessedCallee() != -1) { std::printf("ECHEC guessedCallee à 4 sièges\n"); ++failures; }
        for (int s = 0; s < 4; ++s)
            if (game.knownTeamOf(s, 0) != game.teamOf(s)) { std::printf("ECHEC knownTeamOf à 4 sièges\n"); ++failures; }
        std::printf("4 sièges: guessedCallee=-1, knownTeamOf == teamOf\n");
    }

    std::printf(failures == 0 ? "TOUT OK\n" : "%d ECHEC\n", failures);
    return failures == 0 ? 0 : 1;
}
