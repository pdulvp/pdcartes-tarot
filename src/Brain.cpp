#include "Brain.h"

#include <QCoreApplication>
#include <QRandomGenerator>

#include <algorithm>
#include <array>

namespace {

bool roll(qreal chance) {
    if (chance <= 0.0) return false;
    return QRandomGenerator::global()->generateDouble() < chance;
}

int pickOne(const QVector<int> &list) {
    if (list.isEmpty()) return -1;
    return list.at(QRandomGenerator::global()->bounded(list.size()));
}

bool holds(const QVector<Card> &hand, int rank, Suit suit) {
    for (const Card &card : hand) {
        if (card.suit == suit && card.rank == rank) return true;
    }
    return false;
}

// Les cartes de la main qui portent ces identifiants.
QVector<Card> cardsOf(const QVector<Card> &hand, const QVector<int> &ids) {
    QVector<Card> found;
    for (int id : ids) {
        if (const Card *card = findCard(hand, id)) found.append(*card);
    }
    return found;
}

// Le camp de `seat` tel que `observer` a le droit de le connaître : à 5
// joueurs avec l'appel caché, l'appelé n'est public que lorsqu'il est
// révélé — seul l'appelé lui-même le sait avant. Un expert va plus loin et
// retient sa devinette : le partenaire deviné d'après les cartes données au
// preneur joue avec le preneur, tant que la carte appelée n'a pas parlé.
int knownTeam(const Tarot &game, int seat, int observer, const Skill &skill) {
    const int team = game.knownTeamOf(seat, observer);
    // La devinette ne vaut que pour les autres : chacun connaît son propre
    // camp à coup sûr (l'appelé tient la carte appelée, les autres savent
    // qu'ils ne la tiennent pas).
    if (seat != observer && skill.guessesPartner && team == kDefenceTeam &&
        seat != game.taker() && !game.callRevealed() &&
        seat == game.guessedCallee()) {
        return kTakerTeam;
    }
    return team;
}

} // namespace

// ---------------------------------------------------------------------------
// Les trois niveaux.

Skill skillFor(const Learning &learning, const Innates &innates) {
    Skill skill;
    skill.mastery = masteryOf(learning.xp);

    // Savoir-faire acquis, compétence par compétence : chaque capacité est un
    // rang d'une seule acquise. Les rangs déterminent la force.
    const int lecture = rungOf(learning.pools.at(Learned::Lecture));
    const int memoire = rungOf(learning.pools.at(Learned::Memoire));
    const int defausse = rungOf(learning.pools.at(Learned::Defausse));
    const int maitrise = rungOf(learning.pools.at(Learned::Maitrise));

    skill.weighsHand = lecture >= 1;
    skill.readsTable = memoire >= 1;
    skill.countsCards = memoire >= 2;
    // La devinette de l'appelé caché vient de la mémoire — ou d'une vigilance
    // assez haute pour remarquer seul qui donne des cartes au preneur.
    skill.guessesPartner = memoire >= 2 || innates.at(Innate::Vigilance) >= 2;
    skill.cutsSuit = defausse >= 1;
    skill.savesPetit = maitrise >= 1;
    skill.vigilance = innates.at(Innate::Vigilance);
    skill.prudence = innates.at(Innate::Prudence);

    // La bourde suit la maîtrise rang par rang, et le calme la module : un
    // coup sur trois chez le débutant, aucun chez l'expert imperturbable.
    static const qreal kBlunder[kRungCount] = {0.30, 0.11, 0.035, 0.0};
    static constexpr qreal kBlunderTilt[kInnateTop + 1] = {1.35, 1.00, 0.80, 0.60};
    skill.blunder =
        qBound(0.0, kBlunder[maitrise] * kBlunderTilt[innates.at(Innate::Calme)], 0.30);

    // Traits de tempérament : ils ne rendent ni meilleur ni moins bon, mais
    // reconnaissable. L'audace fixe directement la hardiesse d'enchère — un
    // casse-cou parle sur des mains que le timide laisse passer — sans jamais
    // débloquer un rang d'aucune acquise.
    static constexpr qreal kBoldness[kInnateTop + 1] = {0.82, 0.94, 1.06, 1.18};
    skill.boldness = kBoldness[innates.at(Innate::Audace)];
    return skill;
}

Skill bestSkill() {
    Learning maxed;
    maxed.xp = kMaxExperience;
    for (int i = 0; i < kLearnedCount; ++i) maxed.pools.xp[i] = kPoolCeiling;
    return skillFor(maxed, Innates{});   // tempérament neutre
}

// ---------------------------------------------------------------------------
// Peser une main.
//
// Le compte est celui qu'on fait à une vraie table, exprimé en demi-points
// pour rester entier : un bout vaut deux points, un roi un point, une dame un
// demi, une coupe un point, et la longueur d'atout vaut d'autant plus qu'elle
// est longue. Le petit ne vaut ses deux points que s'il est escorté : seul ou
// presque, c'est un bout qu'on perdra au premier tour d'atout.

int handStrength(const QVector<Card> &hand) {
    int trumps = 0;
    int high = 0;            // les atouts de seize à vingt, qui commandent
    bool petit = false;
    bool vingtEtUn = false;
    bool excuse = false;
    for (const Card &card : hand) {
        if (card.suit != Suit::Atout) continue;
        if (card.excuse()) {
            excuse = true;
            continue;
        }
        ++trumps;
        if (card.rank == kPetit) petit = true;
        if (card.rank == kVingtEtUn) vingtEtUn = true;
        if (card.rank >= 16 && card.rank <= 20) ++high;
    }

    int strength = 0;
    if (vingtEtUn) strength += 4;
    if (excuse) strength += 4;
    if (petit) strength += trumps >= 5 ? 4 : 2;

    // La longueur d'atout. En dessous de cinq elle ne vaut rien : on ne tient
    // pas un contrat sans atout. Au-delà, chaque atout supplémentaire vaut
    // davantage que le précédent, parce que c'est la longueur qui permet de
    // tirer l'atout de la table et de rester maître ensuite.
    static const std::array<int, 11> kLength = {0, 0, 0, 0, 0, 3, 6, 10, 14, 18, 22};
    strength += trumps <= 10 ? kLength[trumps] : 22 + 4 * (trumps - 10);
    strength += high;

    for (int s = 0; s < kSuitCount; ++s) {
        const Suit suit = Suit(s);
        const int length = countSuit(hand, suit);
        if (holds(hand, kRoi, suit)) strength += 2;
        if (holds(hand, kDame, suit)) strength += 1;
        // Une couleur vide est une coupe, et une couleur unique le sera au
        // second tour. Elles ne valent que si l'on a de l'atout pour couper.
        if (trumps >= 5) {
            if (length == 0) strength += 2;
            else if (length == 1) strength += 1;
        }
    }
    return strength;
}

Contract contractFor(int strength, const Skill &skill) {
    // La hardiesse déplace les quatre barres d'un coup : un débutant parle sur
    // des mains que les autres laissent passer.
    const int seen = int(std::lround(strength * skill.boldness));
    if (seen >= kGardeContreStrength) return Contract::GardeContre;
    if (seen >= kGardeSansStrength) return Contract::GardeSans;
    if (seen >= kGardeStrength) return Contract::Garde;
    if (seen >= kPetiteStrength) return Contract::Petite;
    return Contract::Passe;
}

Contract chooseBid(const Tarot &game, int at, const Skill &skill) {
    const QVector<Card> &hand = game.seat(at).hand;

    int strength = handStrength(hand);
    if (!skill.weighsHand) {
        // Un débutant ne pèse pas : il compte ses atouts et ses bouts, et il
        // s'arrête là. Les rois, les dames et les couleurs courtes ne lui
        // disent rien, et il lui manque donc exactement ce qui fait tenir un
        // contrat.
        //
        // Le retranchement de six ramène ce compte-ci à l'échelle de l'autre :
        // mesuré sur cinquante mille mains, il parle à peu près aussi souvent
        // qu'un joueur qui pèse, mais pas sur les mêmes mains. C'est tout le
        // défaut qu'on lui veut -- il prend ce qu'il faut laisser et laisse ce
        // qu'il fallait prendre -- et non celui de parler sans arrêt.
        int trumps = 0;
        for (const Card &card : hand) {
            if (card.suit == Suit::Atout && !card.excuse()) ++trumps;
        }
        strength = std::max(0, 2 * trumps + 4 * countBouts(hand) - 6);
    }

    Contract wanted = contractFor(strength, skill);
    if (wanted == Contract::Passe) return Contract::Passe;

    // On ne parle que pour dépasser ce qui est déjà dit. Une main qui vaut une
    // petite se tait quand la garde est annoncée : annoncer au-dessus de sa
    // main est la façon la plus sûre de chuter.
    const Contract lowest = game.lowestBid();
    if (int(wanted) < int(lowest)) return Contract::Passe;
    if (!game.mayBid(wanted)) return Contract::Passe;
    return wanted;
}

// ---------------------------------------------------------------------------
// L'écart.
//
// Le but n'est pas de mettre des points de côté -- les rois et les bouts sont
// interdits, et ce qui reste ne vaut presque rien -- mais de vider une
// couleur. Une couleur vide, c'est tout le reste de la donne à couper dedans,
// et c'est ce qui fait la différence entre dix-huit cartes et dix-huit cartes
// bien choisies.

Suit chooseCall(const Tarot &game, int at, const Skill &skill) {
    Q_UNUSED(skill);
    // On appelle de préférence une couleur dont on ne tient pas la carte
    // appelée, pour avoir un partenaire ; et parmi celles-là, une couleur où
    // l'on est court, puisque le partenaire en aura vraisemblablement.
    const Seat &who = game.seat(at);
    Suit best = Suit::Pique;
    int bestLen = 99;
    for (int s = 0; s < 4; ++s) {
        const Suit suit = Suit(s);
        const Card called = game.calledCardFor(suit, at);
        if (findCard(who.hand, called.id)) continue;   // on s'appellerait soi-même
        int len = 0;
        for (const Card &card : who.hand) {
            if (card.suit == suit) ++len;
        }
        if (len < bestLen) {
            bestLen = len;
            best = suit;
        }
    }
    return best;
}

QVector<int> chooseEcart(const Tarot &game, int at, const Skill &skill) {
    const QVector<Card> &hand = game.seat(at).hand;

    QVector<Card> free;       // ce qu'on a le droit de jeter
    for (const Card &card : hand) {
        if (game.mayDiscard(card)) free.append(card);
    }

    QVector<int> chosen;
    if (!skill.cutsSuit) {
        // Un débutant jette ses six cartes les moins chères, prises où elles
        // tombent. Il reste long partout, et sans une seule coupe.
        std::sort(free.begin(), free.end(), [](const Card &a, const Card &b) {
            if (cardPoints(a) != cardPoints(b)) return cardPoints(a) < cardPoints(b);
            return cardStrength(a) < cardStrength(b);
        });
        for (const Card &card : free) {
            if (chosen.size() >= game.chienSize()) break;
            chosen.append(card.id);
        }
        return chosen;
    }

    // Les couleurs qu'on peut vider : celles où l'on n'a pas de roi, les
    // courtes d'abord. Un roi interdit de vider sa couleur, puisqu'il reste.
    struct SuitPlan {
        Suit suit;
        int length;
        bool king;
        int points;
    };
    QVector<SuitPlan> plans;
    for (int s = 0; s < kSuitCount; ++s) {
        const Suit suit = Suit(s);
        SuitPlan plan{suit, countSuit(hand, suit), holds(hand, kRoi, suit), 0};
        for (const Card &card : hand) {
            if (card.suit == suit) plan.points += cardPoints(card);
        }
        plans.append(plan);
    }
    std::sort(plans.begin(), plans.end(), [](const SuitPlan &a, const SuitPlan &b) {
        // D'abord celles qu'on peut vider entièrement, de la plus courte à la
        // plus longue ; les couleurs à roi en dernier, où l'on ne prendra que
        // des cartes basses.
        if (a.king != b.king) return b.king;
        return a.length < b.length;
    });

    // Les cartes d'une enseigne qu'on peut jeter, les moins chères d'abord.
    //
    // L'écart compte dans le tas du preneur, et l'on pourrait croire qu'il
    // faut y mettre ce qui vaut : une dame écartée est trois points et demi
    // acquis. Mesuré sur quelques milliers de donnes, c'est perdant -- une
    // dame gardée fait plus souvent son pli qu'elle ne tombe, et une enseigne
    // dont on a retiré ses hautes cartes ne fournit plus rien. On ne jette
    // donc les points que lorsqu'on vide l'enseigne entière, ce que le premier
    // passage fait déjà.
    auto jetables = [&](Suit suit) {
        QVector<Card> list;
        for (const Card &card : free) {
            if (card.suit == suit) list.append(card);
        }
        std::sort(list.begin(), list.end(), [](const Card &a, const Card &b) {
            if (cardPoints(a) != cardPoints(b)) return cardPoints(a) < cardPoints(b);
            return cardStrength(a) < cardStrength(b);
        });
        return list;
    };

    // Premier passage : vider une couleur entière, si elle tient dans les six.
    for (const SuitPlan &plan : plans) {
        if (plan.king || plan.length == 0) continue;
        const QVector<Card> list = jetables(plan.suit);
        if (list.size() != plan.length) continue;          // une carte y est retenue
        if (chosen.size() + list.size() > game.chienSize()) continue;
        for (const Card &card : list) chosen.append(card.id);
    }

    // Second passage : compléter par les cartes les moins chères de ce qui
    // reste, les couleurs sans roi d'abord.
    for (const SuitPlan &plan : plans) {
        if (chosen.size() >= game.chienSize()) break;
        for (const Card &card : jetables(plan.suit)) {
            if (chosen.size() >= game.chienSize()) break;
            if (chosen.contains(card.id)) continue;
            // La carte qui garde un roi ne se jette qu'en dernier recours :
            // un roi seul se coupe, un roi gardé passe.
            if (plan.king && plan.length <= 2) continue;
            chosen.append(card.id);
        }
    }

    // Dernier recours : tout ce qui reste de permis, atouts compris lorsque la
    // main n'offre rien d'autre.
    if (chosen.size() < game.chienSize()) {
        QVector<Card> rest = free;
        std::sort(rest.begin(), rest.end(), [](const Card &a, const Card &b) {
            if ((a.suit == Suit::Atout) != (b.suit == Suit::Atout)) {
                return b.suit == Suit::Atout;      // les atouts en dernier
            }
            if (cardPoints(a) != cardPoints(b)) return cardPoints(a) < cardPoints(b);
            return cardStrength(a) < cardStrength(b);
        });
        for (const Card &card : rest) {
            if (chosen.size() >= game.chienSize()) break;
            if (chosen.contains(card.id)) continue;
            chosen.append(card.id);
        }
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// La poignée et le chelem.

PoigneeKind choosePoignee(const Tarot &game, int at, const Skill &skill) {
    const PoigneeKind kind = game.poigneeAvailable(at);
    if (kind == PoigneeKind::Aucune) return kind;
    // Le preneur la montre toujours : une main qui porte dix atouts est une
    // main qui gagne la donne, et la poignée ira donc de son côté.
    if (at == game.taker()) return kind;
    // Un débutant la montre pour le plaisir — sauf s'il est prudent, auquel
    // cas il la garde pour son camp.
    if (!skill.readsTable) return skill.prudence >= 2 ? PoigneeKind::Aucune : kind;
    // Un défenseur la donne au preneur s'il se trompe : il ne la montre donc
    // que lorsqu'il tient de quoi faire chuter, c'est-à-dire beaucoup d'atout.
    return kind == PoigneeKind::Simple ? PoigneeKind::Aucune : kind;
}

bool chooseChelem(const Tarot &game, int at, const Skill &skill) {
    if (!skill.weighsHand) return false;     // un débutant n'y pense pas
    const QVector<Card> &hand = game.seat(at).hand;
    const int trumps = trumpsInHand(hand);
    if (trumps < 13) return false;
    if (countBouts(hand) < 2) return false;
    int kings = 0;
    for (int s = 0; s < kSuitCount; ++s) {
        if (holds(hand, kRoi, Suit(s))) ++kings;
    }
    // Treize atouts, deux bouts et trois rois : à peu près la seule main sur
    // laquelle les dix-huit plis ne soient pas un pari.
    return kings >= 3 && handStrength(hand) >= kGardeContreStrength;
}

// ---------------------------------------------------------------------------
// La carte à jouer.

namespace {

// Ce qui est déjà passé sur la table, par identifiant de carte.
struct Seen {
    bool gone[kPackSize] = {false};
};

Seen cardsSeen(const Tarot &game) {
    Seen seen;
    for (const Played &played : game.history()) seen.gone[played.card.id] = true;
    return seen;
}

// La plus haute carte d'une enseigne qui n'est ni passée ni dans ma main :
// c'est ce qui reste dehors, et donc ce qui peut encore me prendre un pli.
Card highestOut(const Seen &seen, const QVector<Card> &mine, Suit suit) {
    Card best;
    const int top = suit == Suit::Atout ? kTrumpCount : kRoi;
    for (int rank = top; rank >= kAs; --rank) {
        Card card;
        card.rank = rank;
        card.suit = suit;
        card.id = cardIdFor(rank, suit);
        if (seen.gone[card.id]) continue;
        if (findCard(mine, card.id)) continue;
        best = card;
        break;
    }
    return best;
}

int trumpsOut(const Seen &seen, const QVector<Card> &mine) {
    int left = 0;
    for (int rank = 1; rank <= kTrumpCount; ++rank) {
        const int id = cardIdFor(rank, Suit::Atout);
        if (seen.gone[id]) continue;
        if (findCard(mine, id)) continue;
        ++left;
    }
    return left;
}

// La carte la moins chère d'une liste, et la plus chère.
Card cheapest(const QVector<Card> &list) {
    Card best;
    for (const Card &card : list) {
        if (card.excuse()) continue;         // l'excuse se décide ailleurs
        if (!best.valid() || cardPoints(card) < cardPoints(best) ||
            (cardPoints(card) == cardPoints(best) && cardStrength(card) < cardStrength(best))) {
            best = card;
        }
    }
    return best;
}

Card dearest(const QVector<Card> &list) {
    Card best;
    for (const Card &card : list) {
        if (card.excuse()) continue;
        if (!best.valid() || cardPoints(card) > cardPoints(best) ||
            (cardPoints(card) == cardPoints(best) && cardStrength(card) < cardStrength(best))) {
            best = card;
        }
    }
    return best;
}

// Le petit, quand on l'a et qu'on préfère le garder.
bool isPetit(const Card &card) {
    return card.suit == Suit::Atout && card.rank == kPetit;
}

// Les cartes d'une liste dont on veut bien se séparer : tout sauf le petit,
// qu'on ne joue qu'à défaut d'autre chose.
QVector<Card> withoutPetit(const QVector<Card> &list, bool careful) {
    if (!careful) return list;
    QVector<Card> kept;
    for (const Card &card : list) {
        if (!isPetit(card)) kept.append(card);
    }
    return kept.isEmpty() ? list : kept;
}

} // namespace

int chooseCard(const Tarot &game, int at, const Skill &skill) {
    const QVector<int> legal = game.legalCards(at);
    if (legal.isEmpty()) return -1;
    if (legal.size() == 1) return legal.first();

    // La bourde : un débutant joue franchement la mauvaise carte de temps en
    // temps, et c'est ce qui se voit le plus de l'autre côté de la table.
    if (roll(skill.blunder)) return pickOne(legal);

    const QVector<Card> &hand = game.seat(at).hand;
    const QVector<Card> playable = cardsOf(hand, legal);
    const Seen seen = cardsSeen(game);
    const int myTeam = knownTeam(game, at, at, skill);
    const bool last = game.trick().size() == game.seatCount() - 1;
    const bool lastTrick = game.trickNumber() >= game.trickCount();

    // L'excuse, qui est une décision à elle seule : c'est un bout de quatre
    // points et demi, et on le garde. On la joue sur un pli déjà perdu et
    // seulement quand la carte qu'on jouerait sinon coûte cher -- et jamais au
    // dernier pli, où elle s'en irait avec le pli.
    Card excuse;
    for (const Card &card : playable) {
        if (card.excuse()) excuse = card;
    }

    QVector<Card> others;
    for (const Card &card : playable) {
        if (!card.excuse()) others.append(card);
    }
    if (others.isEmpty()) return excuse.valid() ? excuse.id : legal.first();

    // ---- L'entame.

    if (!game.trickStarted()) {
        const int trumps = trumpsInHand(hand);
        QVector<Card> choice = withoutPetit(others, skill.savesPetit);

        // Le preneur tire l'atout, et il le tire jusqu'au bout : tant qu'un
        // défenseur en a un, c'est lui qui coupera les rois du preneur. On
        // mène par le haut, le vingt et un le premier s'il y est -- il ne se
        // perd pas, et il fait tomber un atout de chaque adversaire.
        //
        // C'est la seule manœuvre du tarot qui gagne des donnes à elle seule,
        // et c'est elle qui sépare un preneur qui tient son contrat d'un
        // preneur qui le chute avec la même main.
        if (at == game.taker() && !lastTrick) {
            const int outside = skill.countsCards ? trumpsOut(seen, hand) : -1;
            const bool worth = outside > 0 ? trumps >= 2 : trumps >= 5;
            if (worth) {
                Card top;
                for (const Card &card : choice) {
                    if (card.suit != Suit::Atout) continue;
                    if (!top.valid() || cardStrength(card) > cardStrength(top)) top = card;
                }
                if (top.valid()) return top.id;
            }
        }

        // Une maîtresse se prend tout de suite : c'est ses points plus ce qui
        // tombe dessus, et attendre, c'est la faire couper. Mais pas tant
        // qu'il reste de l'atout à la table, où elle serait coupée tout de
        // suite -- un joueur qui ne compte pas les cartes attend donc les
        // premiers plis avant d'y aller.
        const bool trumpsDrawn = skill.countsCards ? trumpsOut(seen, hand) <= 4
                                                   : game.trickNumber() >= 5;
        if (trumpsDrawn) {
            // Celui qui compte sait laquelle de ses cartes est devenue
            // maîtresse ; les autres ne connaissent que le roi.
            Card best;
            for (const Card &card : choice) {
                if (card.suit == Suit::Atout) continue;
                const bool master =
                    skill.countsCards
                        ? [&] {
                              const Card out = highestOut(seen, hand, card.suit);
                              return !out.valid() || cardStrength(card) > cardStrength(out);
                          }()
                        : card.rank == kRoi;
                if (!master) continue;
                if (!best.valid() || cardPoints(card) > cardPoints(best)) best = card;
            }
            if (best.valid()) return best.id;
        }

        if (skill.readsTable) {
            // Sinon on mène dans sa couleur la plus longue, par le bas : la
            // carte basse laisse au camp le temps de voir venir, là qu'une
            // dame menée tombe sous le roi.
            Suit longest = Suit::Pique;
            int best = -1;
            for (int s = 0; s < kSuitCount; ++s) {
                const int length = countSuit(hand, Suit(s));
                if (length == 0 || length <= best) continue;
                best = length;
                longest = Suit(s);
            }
            QVector<Card> inSuit;
            for (const Card &card : choice) {
                if (card.suit == longest) inSuit.append(card);
            }
            const Card low = cheapest(inSuit);
            if (low.valid()) return low.id;
        }

        const Card low = cheapest(choice);
        return low.valid() ? low.id : choice.first().id;
    }

    // ---- On suit.

    const int master = game.trickMaster();
    const bool oursHolds = master >= 0 && knownTeam(game, master, at, skill) == myTeam;
    Card top;
    for (const Played &played : game.trick()) {
        if (played.seat == master) top = played.card;
    }
    const Suit led = game.ledSuit();

    // Ce qui bat le pli, parmi ce qu'on a le droit de jouer.
    QVector<Card> winners;
    for (const Card &card : others) {
        if (beatsTrick(card, top, led)) winners.append(card);
    }

    const int pot = game.trickValue();

    if (!skill.readsTable) {
        // Un débutant joue sa carte sans regarder qui tient le pli : il prend
        // quand il peut, de la plus forte carte qu'il a, et se défausse au
        // hasard quand il ne peut pas.
        if (!winners.isEmpty()) {
            Card high;
            for (const Card &card : winners) {
                if (!high.valid() || cardStrength(card) > cardStrength(high)) high = card;
            }
            return high.id;
        }
        return pickOne(legal);
    }

    // Le pli est à nous et il est acquis : on le charge. Quatre points et demi
    // posés sur le pli d'un camarade valent autant qu'un pli pris.
    if (oursHolds) {
        // Acquis quand plus personne ne peut passer devant : soit on est le
        // dernier à parler, soit la carte qui tient le pli est maîtresse de
        // ce qui reste dehors.
        bool sure = last;
        if (!sure && skill.countsCards) {
            const Card out = highestOut(seen, hand, top.suit);
            const bool trumpThreat = top.suit != Suit::Atout && trumpsOut(seen, hand) > 0;
            sure = !trumpThreat && (!out.valid() || cardStrength(top) > cardStrength(out));
        }
        if (sure) {
            // Et c'est ici que le petit se pose. Gardé jusqu'au dernier pli,
            // il est le plus petit atout du jeu et il se perd : un pli déjà
            // acquis à son camp est le seul endroit où il arrive à bon port.
            // Les quatre points et demi du bout, et le contrat abaissé de
            // dix, tiennent à ce coup-là.
            const Card rich = dearest(others);
            if (rich.valid()) return rich.id;
        }
        // Pas encore acquis : on attend, et on met la moins chère.
        const Card low = cheapest(withoutPetit(others, skill.savesPetit));
        if (low.valid()) return low.id;
    }

    // Le pli est à l'autre camp. On le prend si on peut et si cela vaut la
    // peine, ce qui n'est pas la même chose : prendre au plus juste quand il
    // reste des joueurs derrière, c'est se faire repasser dessus et perdre la
    // carte en plus du pli.
    if (!winners.isEmpty()) {
        QVector<Card> usable = withoutPetit(winners, skill.savesPetit && !lastTrick);
        std::sort(usable.begin(), usable.end(),
                  [](const Card &a, const Card &b) { return cardStrength(a) < cardStrength(b); });

        // Dernier à parler : rien ne peut plus repasser dessus, et la plus
        // petite carte qui gagne suffit.
        if (last) return usable.first().id;

        // La plus petite de nos cartes qui soit maîtresse, c'est-à-dire qui
        // passe devant tout ce qui reste dehors. C'est la carte à jouer : le
        // pli est gagné, et rien de plus cher n'est dépensé.
        if (skill.countsCards) {
            const Suit family = usable.first().suit == Suit::Atout ? Suit::Atout : led;
            const Card out = highestOut(seen, hand, family);
            for (const Card &card : usable) {
                if (!out.valid() || cardStrength(card) > cardStrength(out)) return card.id;
            }
        }

        // Rien de maître. Un pli qui vaut la peine se dispute quand même, et
        // par le haut ; un pli vide se laisse filer, et la carte qui l'aurait
        // pris sera jouée plus tard, quand elle ne risquera plus d'être
        // coupée ou surmontée. C'est le coup que les débutants ne font
        // jamais : ils prennent parce qu'ils peuvent.
        //
        // Le preneur fait exception tant qu'il lui reste de l'atout à tirer.
        // Il ne peut tirer que s'il mène, et il ne mène que s'il prend un
        // pli : la main vaut alors plus que la carte qu'elle coûte, fût-ce
        // sur un pli vide. C'est toute la différence entre un preneur qui
        // attend d'être servi et un preneur qui prend la donne en main.
        const int trumps = trumpsInHand(hand);
        const bool drawing = at == game.taker() && trumps >= 4 && !lastTrick &&
                             (!skill.countsCards || trumpsOut(seen, hand) > 0);
        if (pot >= 10 || lastTrick) return usable.last().id;
        if (drawing) return usable.first().id;
    }

    // On ne peut pas le prendre. L'excuse se place ici : un pli perdu sur
    // lequel on allait poser une figure est exactement ce qu'elle évite.
    const Card low = cheapest(withoutPetit(others, skill.savesPetit));
    if (excuse.valid() && !lastTrick && skill.readsTable && low.valid() &&
        cardPoints(low) >= 5) {
        return excuse.id;
    }
    return low.valid() ? low.id : others.first().id;
}

// ---------------------------------------------------------------------------
// Le conseil.

QString advise(const Tarot &game, int at) {
    const Skill skill = bestSkill();
    const Seat &who = game.seat(at);

    if (game.phase() == Phase::Bidding) {
        const int strength = handStrength(who.hand);
        const Contract wanted = contractFor(strength, skill);
        const QString worth =
            QCoreApplication::translate("brain", "Votre main vaut %1 points de prise")
                .arg(pointsText(strength));
        if (wanted == Contract::Passe) {
            return QCoreApplication::translate("brain",
                                               "%1, et il en faut %2 pour une petite : passez.")
                .arg(worth, pointsText(kPetiteStrength));
        }
        if (!game.mayBid(wanted)) {
            return QCoreApplication::translate(
                       "brain", "%1, de quoi annoncer une %2 — mais %3 est déjà dit : passez.")
                .arg(worth, contractName(wanted), contractName(game.bid()));
        }
        return QCoreApplication::translate("brain", "%1 : annoncez %2.")
            .arg(worth, contractName(wanted));
    }

    if (game.phase() == Phase::Ecart) {
        const QVector<int> ecart = chooseEcart(game, at, skill);
        QStringList named;
        for (int id : ecart) {
            if (const Card *card = findCard(who.hand, id)) named.append(cardLabel(*card));
        }
        return QCoreApplication::translate("brain", "Écartez %1 : une couleur vidée est une "
                                                    "couleur où l'on coupe.")
            .arg(named.join(QStringLiteral(" ")));
    }

    if (game.phase() != Phase::Playing) {
        return QCoreApplication::translate("brain", "Il n'y a rien à décider pour l'instant.");
    }

    const int id = chooseCard(game, at, skill);
    const Card *card = findCard(who.hand, id);
    if (!card) return QCoreApplication::translate("brain", "Jouez ce que vous voulez.");

    QString why;
    if (!game.trickStarted()) {
        if (at == game.taker() && card->suit == Suit::Atout) {
            why = QCoreApplication::translate("brain",
                                              "tirez l'atout tant qu'il en reste dehors");
        } else if (card->rank == kRoi && card->suit != Suit::Atout) {
            why = QCoreApplication::translate("brain", "un roi se prend avant qu'on le coupe");
        } else {
            why = QCoreApplication::translate("brain", "entamez bas, et voyez venir");
        }
    } else {
        const int master = game.trickMaster();
        const bool ours = master >= 0 && knownTeam(game, master, at, skill) == knownTeam(game, at, at, skill);
        if (ours && cardPoints(*card) >= 5) {
            why = QCoreApplication::translate("brain", "le pli est à votre camp : chargez-le");
        } else if (beatsTrick(*card, game.trick().isEmpty() ? Card{} : game.trick().first().card,
                              game.ledSuit())) {
            why = QCoreApplication::translate("brain", "prenez le pli au plus juste");
        } else if (card->excuse()) {
            why = QCoreApplication::translate("brain",
                                              "l'excuse vous reste : posez-la sur un pli perdu");
        } else {
            why = QCoreApplication::translate("brain", "ce pli est perdu : n'y laissez rien");
        }
    }
    return QCoreApplication::translate("brain", "Jouez %1 — %2.").arg(cardName(*card), why);
}
