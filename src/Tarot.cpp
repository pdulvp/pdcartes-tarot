#include "Tarot.h"

#include <QCoreApplication>
#include <QStringList>

#include <algorithm>

// ---------------------------------------------------------------------------
// Les cinq paroles de l'enchère.

int contractMultiplier(Contract contract) {
    switch (contract) {
    case Contract::Petite: return 1;
    case Contract::Garde: return 2;
    case Contract::GardeSans: return 4;
    case Contract::GardeContre: return 6;
    case Contract::Passe: break;
    }
    return 0;
}

QString contractName(Contract contract) {
    switch (contract) {
    case Contract::Petite: return QCoreApplication::translate("tarot", "petite");
    case Contract::Garde: return QCoreApplication::translate("tarot", "garde");
    case Contract::GardeSans:
        return QCoreApplication::translate("tarot", "garde sans le chien");
    case Contract::GardeContre:
        return QCoreApplication::translate("tarot", "garde contre le chien");
    case Contract::Passe: break;
    }
    return QCoreApplication::translate("tarot", "passe");
}

QString contractShort(Contract contract) {
    switch (contract) {
    case Contract::Petite: return QCoreApplication::translate("tarot", "petite");
    case Contract::Garde: return QCoreApplication::translate("tarot", "garde");
    case Contract::GardeSans: return QCoreApplication::translate("tarot", "garde sans");
    case Contract::GardeContre: return QCoreApplication::translate("tarot", "garde contre");
    case Contract::Passe: break;
    }
    return QCoreApplication::translate("tarot", "passe");
}

bool contractTakesChien(Contract contract) {
    return contract == Contract::Petite || contract == Contract::Garde;
}

// ---------------------------------------------------------------------------

Tarot::Tarot(QObject *parent) : QObject(parent) {}

const Seat &Tarot::seat(int at) const {
    static const Seat empty;
    if (at < 0 || at >= m_seats.size()) return empty;
    return m_seats.at(at);
}

Seat &Tarot::here() { return m_seats[qBound(0, m_turn, m_seats.size() - 1)]; }

bool Tarot::ledKnown() const {
    for (const Played &played : m_trick) {
        if (!played.card.excuse()) return true;
    }
    return false;
}

Suit Tarot::ledSuit() const {
    // L'enseigne demandée est celle de la première carte qui en a une.
    // L'excuse posée la première ne demande rien : c'est la carte suivante
    // qui fixe la couleur, et les joueurs d'après fournissent à celle-là.
    for (const Played &played : m_trick) {
        if (!played.card.excuse()) return played.card.suit;
    }
    return Suit::Pique;
}

int Tarot::trickMaster() const {
    int best = -1;
    Card top;
    const Suit led = ledSuit();
    for (const Played &played : m_trick) {
        if (!beatsTrick(played.card, top, led)) continue;
        top = played.card;
        best = played.seat;
    }
    return best;
}

int Tarot::trickValue() const {
    int sum = 0;
    for (const Played &played : m_trick) sum += cardPoints(played.card);
    return sum;
}

int Tarot::seatScore(int at) const {
    if (at < 0 || at >= kSeatCount) return 0;
    return m_score[at];
}

int Tarot::teamPoints(int team) const {
    return team >= 0 && team < kTeamCount ? m_points[team] : 0;
}

int Tarot::teamTricks(int team) const {
    return team >= 0 && team < kTeamCount ? m_tricks[team] : 0;
}

int Tarot::takerBouts() const { return m_bouts[kTakerTeam]; }

int Tarot::takerTarget() const { return targetFor(takerBouts()); }

QString Tarot::teamName(int team) const {
    if (team == kTakerTeam) {
        if (m_taker < 0) return tr("Le preneur");
        return seat(m_taker).human() ? tr("Vous") : seat(m_taker).name;
    }
    return tr("La défense");
}

void Tarot::sortSeatHand(int at, bool byValue) {
    if (at < 0 || at >= m_seats.size()) return;
    if (byValue) {
        sortByValue(m_seats[at].hand);
    } else {
        sortBySuit(m_seats[at].hand);
    }
    Q_EMIT changed();
}

// ---------------------------------------------------------------------------
// La partie, et la donne.

void Tarot::startMatch(const QVector<SeatSpec> &specs, int donnes, bool withPoignees,
                       bool withChelem) {
    m_seats.clear();
    for (const SeatSpec &spec : specs) {
        Seat seat;
        seat.rivalId = spec.rivalId;
        seat.name = spec.name;
        seat.hue = spec.hue;
        m_seats.append(seat);
    }
    m_withPoignees = withPoignees;
    m_withChelem = withChelem;
    m_donnes = std::max(1, donnes);
    m_donneNumber = 0;
    m_dealsGiven = 0;
    m_matchWinner = -1;
    for (int at = 0; at < kSeatCount; ++at) m_score[at] = 0;
    // Le donneur ne parle pas le premier, alors c'est la dernière place qui
    // donne : la première chose qui arrive dans une partie est une décision du
    // joueur, et non trois adversaires qui réfléchissent.
    m_dealer = m_seats.size() - 1;
    startDonne();
}

void Tarot::startDonne() {
    if (m_seats.size() != kSeatCount) return;
    // La donne tourne d'une place à chaque fois, et l'entame avec elle. Une
    // donne annulée a déjà fait tourner le donneur en s'annulant : on ne le
    // fait donc tourner ici que si la précédente a été jouée.
    if (m_donneNumber > 0 || m_result.annulled) m_dealer = (m_dealer + 1) % kSeatCount;

    m_trick.clear();
    m_lastTrick.clear();
    m_history.clear();
    m_chien.clear();
    m_ecart.clear();
    m_taker = -1;
    m_contract = Contract::Passe;
    m_ecartNeeded = false;
    m_chelemAnnounced = false;
    m_trickNumber = 0;
    m_trickWinner = -1;
    m_petitAuBoutTeam = -1;
    for (int team = 0; team < kTeamCount; ++team) {
        m_points[team] = 0;
        m_bouts[team] = 0;
        m_tricks[team] = 0;
        m_extra[team] = 0;
        m_extraBouts[team] = 0;
    }
    m_result = DonneResult{};

    for (Seat &seat : m_seats) {
        seat.hand.clear();
        seat.bid = Contract::Passe;
        seat.passed = false;
        seat.said.clear();
        seat.poignee = PoigneeKind::Aucune;
        seat.poigneeShown.clear();
        seat.declared = false;
    }

    m_bidsThisRound = 0;
    m_bid = Contract::Passe;
    m_bidder = -1;

    m_redeals = 0;
    ++m_dealsGiven;
    deal();
    // Le petit sec : une main dont le petit est le seul atout et qui n'a pas
    // l'excuse ne se joue pas. Son détenteur serait forcé de le jouer au
    // premier atout demandé et le perdrait sans y pouvoir rien, ce qui coûte
    // un bout à son camp sur un coup de dé.
    while (petitSec()) {
        ++m_redeals;
        deal();
    }

    const QString dealer = m_seats.at(m_dealer).human()
                               ? tr("Vous donnez")
                               : tr("%1 donne").arg(m_seats.at(m_dealer).name);
    if (m_redeals > 0) {
        Q_EMIT acted(m_dealer, tr("%1. Petit sec : on remélange et on redonne.").arg(dealer));
    } else {
        Q_EMIT acted(m_dealer, tr("%1 : dix-huit cartes chacun, six au chien.").arg(dealer));
    }

    m_turn = (m_dealer + 1) % kSeatCount;
    m_phase = Phase::Bidding;
    Q_EMIT changed();
    Q_EMIT turnBegan(m_turn);
}

void Tarot::deal() {
    for (Seat &seat : m_seats) seat.hand.clear();
    m_chien.clear();

    QVector<Card> stock = freshPack();
    shuffleCards(stock);

    // Par paquets de trois, en tournant depuis la gauche du donneur, et une
    // carte au chien de temps en temps. Le chien se compose pendant la
    // distribution et non à la fin, et jamais de la première ni de la
    // dernière carte du jeu : c'est la règle, et elle est là pour qu'un
    // donneur ne puisse pas savoir ce qu'il y met.
    //
    // Les six cartes du chien sont posées après le premier paquet et avant le
    // dernier, réparties au long de la distribution.
    const int packets = (kSeatCount * kHandSize) / kDealPacket;   // vingt-quatre
    int chienAt = 0;
    const int chienEvery = packets / (kChienSize + 1);
    for (int packet = 0; packet < packets; ++packet) {
        if (m_chien.size() < kChienSize && packet > 0 && packet % chienEvery == 0 &&
            packet < packets - 1) {
            m_chien.append(stock.takeLast());
        }
        const int who = (m_dealer + 1 + packet % kSeatCount) % kSeatCount;
        for (int card = 0; card < kDealPacket; ++card) m_seats[who].hand.append(stock.takeLast());
    }
    // Ce qui reste va au chien : le compte retombe toujours juste, les
    // paquets et le chien étant fixés d'avance.
    while (m_chien.size() < kChienSize && !stock.isEmpty()) m_chien.append(stock.takeLast());

    for (Seat &seat : m_seats) sortBySuit(seat.hand);
}

bool Tarot::petitSec() const {
    for (const Seat &seat : m_seats) {
        int trumps = 0;
        bool petit = false;
        bool excuse = false;
        for (const Card &card : seat.hand) {
            if (card.suit != Suit::Atout) continue;
            if (card.excuse()) {
                excuse = true;
                continue;
            }
            ++trumps;
            if (card.rank == kPetit) petit = true;
        }
        if (petit && trumps == 1 && !excuse) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// L'enchère.

Contract Tarot::lowestBid() const {
    if (m_bidder < 0) return Contract::Petite;
    return Contract(int(m_bid) + 1);
}

bool Tarot::mayBid(Contract contract) const {
    if (m_phase != Phase::Bidding) return false;
    if (contract == Contract::Passe) return false;
    return int(contract) >= int(lowestBid()) && int(contract) < kContractCount;
}

bool Tarot::makeBid(Contract contract, QString *why) {
    if (m_phase != Phase::Bidding) {
        if (why) *why = tr("Ce n'est pas le moment d'enchérir.");
        return false;
    }
    if (!mayBid(contract)) {
        if (why) {
            *why = m_bidder < 0
                       ? tr("Ce contrat n'existe pas.")
                       : tr("Il faut dépasser %1, qui est déjà annoncé.")
                             .arg(contractName(m_bid));
        }
        return false;
    }

    Seat &who = here();
    who.bid = contract;
    who.passed = false;
    who.said = contractShort(contract);
    m_bid = contract;
    m_bidder = m_turn;

    Q_EMIT acted(m_turn, who.human() ? tr("Vous annoncez %1.").arg(contractName(contract))
                                     : tr("%1 annonce %2.")
                                           .arg(who.name, contractName(contract)));
    nextBidder();
    return true;
}

bool Tarot::pass(QString *why) {
    if (m_phase != Phase::Bidding) {
        if (why) *why = tr("Ce n'est pas le moment de parler.");
        return false;
    }
    Seat &who = here();
    who.passed = true;
    who.said = tr("passe");
    Q_EMIT acted(m_turn, who.human() ? tr("Vous passez.") : tr("%1 passe.").arg(who.name));
    nextBidder();
    return true;
}

void Tarot::nextBidder() {
    ++m_bidsThisRound;
    // Un seul tour de table : chacun parle une fois, et le plus offrant joue.
    // C'est ce qui sépare l'enchère du tarot de celle de la coinche, où l'on
    // repasse tant que quelqu'un surenchérit.
    if (m_bidsThisRound < kSeatCount) {
        m_turn = (m_turn + 1) % kSeatCount;
        Q_EMIT changed();
        Q_EMIT turnBegan(m_turn);
        return;
    }
    settleBidding();
}

void Tarot::settleBidding() {
    if (m_bidder < 0) {
        annulDonne();
        return;
    }

    m_taker = m_bidder;
    m_contract = m_bid;
    m_ecartNeeded = contractTakesChien(m_contract);
    const Seat &who = seat(m_taker);

    if (contractTakesChien(m_contract)) {
        // Le chien est retourné et vu de tous : c'est le moment où la table
        // apprend six cartes d'un coup, et il faut le laisser durer.
        m_phase = Phase::Chien;
        QStringList named;
        for (const Card &card : m_chien) named.append(cardLabel(card));
        Q_EMIT acted(m_taker, tr("%1 prend à la %2. Le chien : %3.")
                                  .arg(who.human() ? tr("Vous") : who.name,
                                       contractName(m_contract), named.join(QStringLiteral(" "))));
        Q_EMIT changed();
        return;
    }

    // Garde sans, garde contre : le chien ne se montre pas. Il va au tas du
    // preneur dans un cas, à celui des défenseurs dans l'autre, et il y est
    // compté à la fin sans que personne l'ait vu.
    const int side = m_contract == Contract::GardeSans ? kTakerTeam : kDefenceTeam;
    m_extra[side] += totalPoints(m_chien);
    m_extraBouts[side] += countBouts(m_chien);
    Q_EMIT acted(m_taker, tr("%1 prend : %2. Le chien ne sera pas vu.")
                              .arg(who.human() ? tr("Vous") : who.name,
                                   contractName(m_contract)));
    m_chien.clear();
    // Le chien n'étant ni montré ni pris, il n'y a pas d'écart à faire. La
    // phase est traversée quand même : c'est là que le chelem s'annonce.
    m_phase = Phase::Ecart;
    m_turn = m_taker;
    Q_EMIT changed();
    Q_EMIT turnBegan(m_turn);
}

void Tarot::annulDonne() {
    // Les quatre ont passé : la donne ne se joue pas, et c'est au suivant de
    // donner. Ça arrive, et ce n'est pas un échec du programme -- c'est une
    // table où personne n'avait de quoi prendre.
    m_phase = Phase::DonneOver;
    m_result = DonneResult{};
    m_result.played = false;
    m_result.annulled = true;
    Q_EMIT acted(-1, tr("Les quatre ont passé : la donne est annulée."));
    Q_EMIT changed();
    Q_EMIT donneFinished();
}

// ---------------------------------------------------------------------------
// Le chien et l'écart.

void Tarot::chienSeen() {
    if (m_phase != Phase::Chien) return;
    takeChien();
}

void Tarot::takeChien() {
    // Le chien entre dans la main du preneur, qui en tient vingt-quatre le
    // temps de choisir les six qu'il met de côté.
    for (const Card &card : m_chien) m_seats[m_taker].hand.append(card);
    m_chien.clear();
    sortBySuit(m_seats[m_taker].hand);
    m_phase = Phase::Ecart;
    m_turn = m_taker;
    Q_EMIT changed();
    Q_EMIT turnBegan(m_turn);
}

int Tarot::forcedTrumpsInEcart() const {
    if (m_taker < 0 || !m_ecartNeeded) return 0;
    // Les cartes qu'on a le droit de jeter sans réserve : ni roi, ni bout, ni
    // atout. S'il y en a moins de six, il faut compléter avec des atouts, et
    // c'est seulement dans ce cas qu'un atout peut être écarté.
    int free = 0;
    for (const Card &card : seat(m_taker).hand) {
        if (card.suit == Suit::Atout) continue;
        if (card.rank == kRoi) continue;
        ++free;
    }
    return std::max(0, kChienSize - free);
}

bool Tarot::mayDiscard(const Card &card) const {
    if (!card.valid() || !m_ecartNeeded || m_phase != Phase::Ecart) return false;
    // Ni les rois ni les bouts : ce sont les cartes qui décident de la donne,
    // et les mettre de côté reviendrait à se les garantir.
    if (isBout(card)) return false;
    if (card.suit != Suit::Atout && card.rank == kRoi) return false;
    if (card.suit == Suit::Atout) return forcedTrumpsInEcart() > 0;
    return true;
}

bool Tarot::setEcart(const QVector<int> &cardIds, QString *why) {
    if (m_phase != Phase::Ecart || !m_ecartNeeded) {
        if (why) *why = tr("Ce n'est pas le moment de faire son écart.");
        return false;
    }
    if (cardIds.size() != kChienSize) {
        if (why) *why = tr("L'écart est de six cartes, ni plus ni moins.");
        return false;
    }

    const int forced = forcedTrumpsInEcart();
    QVector<Card> chosen;
    int trumps = 0;
    for (int id : cardIds) {
        const Card *card = findCard(seat(m_taker).hand, id);
        if (!card) {
            if (why) *why = tr("Cette carte n'est pas dans votre main.");
            return false;
        }
        if (isBout(*card)) {
            if (why) *why = tr("%1 est un bout : un bout ne s'écarte pas.").arg(cardName(*card));
            return false;
        }
        if (card->suit != Suit::Atout && card->rank == kRoi) {
            if (why) *why = tr("Un roi ne s'écarte pas.");
            return false;
        }
        if (card->suit == Suit::Atout) ++trumps;
        chosen.append(*card);
    }
    if (trumps != forced) {
        if (why) {
            *why = forced == 0
                       ? tr("On n'écarte un atout que lorsqu'on n'a rien d'autre à jeter.")
                       : tr("Il vous manque des cartes à jeter : l'écart doit contenir "
                            "exactement %n atout(s).", nullptr, forced);
        }
        return false;
    }

    for (const Card &card : chosen) takeCard(m_seats[m_taker].hand, card.id);
    m_ecart = chosen;
    // L'écart est au preneur : il compte dans son tas à la fin, sans que
    // personne l'ait vu.
    m_extra[kTakerTeam] += totalPoints(m_ecart);
    m_extraBouts[kTakerTeam] += countBouts(m_ecart);

    const Seat &who = seat(m_taker);
    if (trumps > 0) {
        // Un atout écarté se montre : c'est une faveur faite aux défenseurs,
        // et la contrepartie du droit de le jeter.
        QStringList named;
        for (const Card &card : chosen) {
            if (card.suit == Suit::Atout) named.append(cardName(card));
        }
        Q_EMIT acted(m_taker, tr("%1 écarte et montre %2.")
                                  .arg(who.human() ? tr("Vous") : who.name,
                                       named.join(tr(" et "))));
    } else {
        Q_EMIT acted(m_taker, who.human() ? tr("Vous faites votre écart.")
                                          : tr("%1 fait son écart.").arg(who.name));
    }
    Q_EMIT changed();
    return true;
}

bool Tarot::mayAnnounceChelem(int at) const {
    return m_withChelem && m_phase == Phase::Ecart && at == m_taker && !m_chelemAnnounced &&
           ecartDone();
}

bool Tarot::announceChelem(QString *why) {
    if (!mayAnnounceChelem(m_taker)) {
        if (why) *why = tr("Ce n'est pas le moment d'annoncer le chelem.");
        return false;
    }
    m_chelemAnnounced = true;
    const Seat &who = seat(m_taker);
    Q_EMIT acted(m_taker, who.human() ? tr("Vous annoncez le chelem : les dix-huit plis.")
                                      : tr("%1 annonce le chelem.").arg(who.name));
    Q_EMIT changed();
    return true;
}

void Tarot::finishEcart() {
    if (m_phase != Phase::Ecart || !ecartDone()) return;
    beginPlay();
}

void Tarot::beginPlay() {
    m_phase = Phase::Playing;
    m_trickNumber = 1;
    // L'entame est à la droite du donneur, sauf chelem annoncé : celui qui
    // s'engage aux dix-huit plis entame, parce qu'il ne peut en tenir aucun
    // s'il ne mène pas le premier.
    m_turn = m_chelemAnnounced ? m_taker : (m_dealer + 1) % kSeatCount;
    for (Seat &seat : m_seats) sortBySuit(seat.hand);
    Q_EMIT changed();
    Q_EMIT turnBegan(m_turn);
}

// ---------------------------------------------------------------------------
// La poignée.

PoigneeKind Tarot::poigneeAvailable(int at) const {
    if (!m_withPoignees) return PoigneeKind::Aucune;
    if (at < 0 || at >= m_seats.size()) return PoigneeKind::Aucune;
    return bestPoignee(seat(at).hand);
}

bool Tarot::mayShowPoignee(int at) const {
    if (m_phase != Phase::Playing || m_trickNumber != 1) return false;
    if (at < 0 || at >= m_seats.size()) return false;
    return !seat(at).declared && poigneeAvailable(at) != PoigneeKind::Aucune;
}

bool Tarot::showPoignee(int at, PoigneeKind kind) {
    if (at < 0 || at >= m_seats.size()) return false;
    Seat &who = m_seats[at];
    if (who.declared) return false;
    who.declared = true;
    if (kind == PoigneeKind::Aucune) return true;
    if (!m_withPoignees) return true;
    const QVector<Card> shown = poigneeCards(who.hand, kind);
    if (shown.isEmpty()) return false;

    who.poignee = kind;
    who.poigneeShown = shown;
    Q_EMIT acted(at, who.human()
                         ? tr("Vous montrez une %1 : %2 atouts posés sur la table.")
                               .arg(poigneeName(kind))
                               .arg(shown.size())
                         : tr("%1 montre une %2 : %3 atouts.")
                               .arg(who.name, poigneeName(kind))
                               .arg(shown.size()));
    Q_EMIT changed();
    return true;
}

// ---------------------------------------------------------------------------
// Ce qu'on a le droit de jouer.
//
// Trois règles, et elles sont tout le tarot :
//
//   fournir   on joue l'enseigne demandée si on l'a.
//   couper    si on ne l'a pas, il faut jouer atout -- toujours, et sans
//             égard pour qui tient le pli. C'est ce qui distingue le tarot
//             de la belote, où le partenaire qui tient le pli dispense de
//             couper : au tarot le preneur est seul, et les trois défenseurs
//             se doivent l'atout les uns aux autres.
//   monter    sur un atout déjà posé, il faut passer dessus si on peut. Si on
//             ne le peut pas, il faut quand même fournir un atout.
//
// Et une exception qui ne ressemble à rien d'autre : l'excuse se joue à tout
// moment, à la place de n'importe quelle carte, et sans rien devoir à
// personne.

QVector<int> Tarot::legalCards(int at) const {
    QVector<int> allowed;
    const Seat &who = seat(at);
    if (m_phase != Phase::Playing) return allowed;

    auto everything = [&] {
        for (const Card &card : who.hand) allowed.append(card.id);
        return allowed;
    };
    // L'excuse est jouable dans tous les cas : elle est ajoutée à chaque
    // liste, plutôt que répétée dans chaque branche.
    auto withExcuse = [&](QVector<int> list) {
        for (const Card &card : who.hand) {
            if (card.excuse()) list.append(card.id);
        }
        return list;
    };

    // Celui qui entame joue ce qu'il veut ; et celui qui suit une excuse
    // entamée aussi, puisque c'est lui qui demande l'enseigne.
    if (m_trick.isEmpty() || !ledKnown()) return everything();

    const Suit led = ledSuit();
    const bool hasLed = led != Suit::Atout && hasSuit(who.hand, led);
    const bool hasTrump = hasSuit(who.hand, Suit::Atout);

    // Le plus fort atout déjà sur la table, s'il y en a un.
    Card bestTrump;
    for (const Played &played : m_trick) {
        if (played.card.suit != Suit::Atout || played.card.excuse()) continue;
        if (!bestTrump.valid() || cardStrength(played.card) > cardStrength(bestTrump)) {
            bestTrump = played.card;
        }
    }

    // Les atouts de la main, et ceux qui passent devant ce qui est posé.
    auto trumpsAbove = [&](const Card &floor) {
        QVector<int> above;
        for (const Card &card : who.hand) {
            if (card.suit != Suit::Atout || card.excuse()) continue;
            if (!floor.valid() || cardStrength(card) > cardStrength(floor)) above.append(card.id);
        }
        return above;
    };

    if (led != Suit::Atout && hasLed) {
        for (const Card &card : who.hand) {
            if (card.suit == led) allowed.append(card.id);
        }
        return withExcuse(allowed);
    }

    if (!hasTrump) {
        // Ni l'enseigne demandée ni d'atout : on se défausse de ce qu'on veut.
        return everything();
    }

    const QVector<int> above = trumpsAbove(bestTrump);
    if (!above.isEmpty()) return withExcuse(above);
    // On ne peut pas monter : il faut quand même fournir un atout, ce qu'on
    // appelle sous-couper. Lequel est libre.
    for (const Card &card : who.hand) {
        if (card.suit == Suit::Atout && !card.excuse()) allowed.append(card.id);
    }
    return withExcuse(allowed);
}

bool Tarot::legal(int at, int cardId, QString *why) const {
    const Seat &who = seat(at);
    const Card *card = findCard(who.hand, cardId);
    if (!card) {
        if (why) *why = tr("Cette carte n'est pas dans votre main.");
        return false;
    }
    if (m_phase != Phase::Playing) {
        if (why) *why = tr("Ce n'est pas le moment de jouer une carte.");
        return false;
    }
    if (legalCards(at).contains(cardId)) return true;
    if (!why) return false;
    // Tout est permis à l'entame, donc un refus veut dire qu'il y a un pli en
    // cours ; la garde est là pour que ce raisonnement soit écrit quelque part
    // plutôt que supposé.
    if (m_trick.isEmpty() || !ledKnown()) return false;

    // Pourquoi pas. C'est la moitié de l'intérêt de la fonction : une carte
    // qu'on ne peut pas jouer et dont on ne sait pas pourquoi est une règle
    // qu'on n'apprendra pas.
    const Suit led = ledSuit();
    Card bestTrump;
    for (const Played &played : m_trick) {
        if (played.card.suit != Suit::Atout || played.card.excuse()) continue;
        if (!bestTrump.valid() || cardStrength(played.card) > cardStrength(bestTrump)) {
            bestTrump = played.card;
        }
    }

    if (led != Suit::Atout && hasSuit(who.hand, led)) {
        *why = tr("%1 est demandé, et vous en avez : il faut fournir à la couleur.")
                   .arg(suitName(led));
        return false;
    }
    if (card->suit != Suit::Atout) {
        *why = led == Suit::Atout
                   ? tr("L'atout est demandé, et vous en avez : il faut fournir.")
                   : tr("Vous n'avez pas de %1 : il faut couper, et vous avez de l'atout.")
                         .arg(suitName(led));
        return false;
    }
    *why = tr("Sur un atout posé il faut monter si on peut : %1 ne passe pas devant %2.")
               .arg(cardName(*card), cardName(bestTrump));
    return false;
}

// ---------------------------------------------------------------------------
// Le pli.

bool Tarot::play(int cardId, QString *why) {
    if (m_phase != Phase::Playing) {
        if (why) *why = tr("Ce n'est pas le moment de jouer une carte.");
        return false;
    }
    if (!legal(m_turn, cardId, why)) return false;

    const int at = m_turn;
    // La poignée se montre avant sa première carte, et pas après : jouer sans
    // l'avoir montrée, c'est y renoncer.
    m_seats[at].declared = true;

    const Card card = takeCard(here().hand, cardId);
    m_trick.append({at, card});
    m_history.append({at, card});

    Q_EMIT acted(at, seat(at).human() ? tr("Vous jouez %1.").arg(cardName(card))
                                      : tr("%1 joue %2.").arg(seat(at).name, cardName(card)));

    if (m_trick.size() < kSeatCount) {
        m_turn = (m_turn + 1) % kSeatCount;
        Q_EMIT changed();
        Q_EMIT turnBegan(m_turn);
        return true;
    }

    m_trickWinner = trickMaster();
    m_phase = Phase::TrickOver;
    Q_EMIT changed();
    Q_EMIT trickFinished(m_trickWinner);
    return true;
}

void Tarot::sweepTrick() {
    if (m_phase != Phase::TrickOver) return;

    const int winner = m_trickWinner;
    const int team = teamOf(winner);
    const bool last = m_trickNumber >= kTrickCount;

    // L'excuse, qui est la seule carte à ne pas suivre le pli.
    //
    // Elle reste au camp qui l'a jouée, lequel rend en échange une carte
    // basse -- un demi-point -- au camp qui a pris le pli. Au dernier pli il
    // n'y a plus rien à rendre et l'excuse s'en va avec le pli, sauf si son
    // camp a pris les dix-sept premiers : dans ce cas il fait chelem et la
    // garde.
    int excuseTeam = -1;
    bool excuseKept = false;
    for (const Played &played : m_trick) {
        if (!played.card.excuse()) continue;
        excuseTeam = teamOf(played.seat);
        excuseKept = !last || m_tricks[excuseTeam] == kTrickCount - 1;
    }

    for (const Played &played : m_trick) {
        const int side =
            (played.card.excuse() && excuseKept) ? excuseTeam : team;
        m_points[side] += cardPoints(played.card);
        if (isBout(played.card)) ++m_bouts[side];
    }
    if (excuseTeam >= 0 && excuseKept && excuseTeam != team) {
        // La carte rendue : un demi-point qui passe d'un tas à l'autre.
        m_points[excuseTeam] -= 1;
        m_points[team] += 1;
    }

    if (last) {
        // Le petit au bout : le petit mené au dernier pli vaut dix points au
        // camp qui prend ce pli-là. C'est la seule raison de garder le plus
        // petit atout du jeu jusqu'au bout.
        for (const Played &played : m_trick) {
            if (played.card.suit == Suit::Atout && played.card.rank == kPetit) {
                m_petitAuBoutTeam = team;
            }
        }
    }

    ++m_tricks[team];
    m_lastTrick = m_trick;
    m_trick.clear();

    if (last) {
        finishDonne();
        return;
    }

    ++m_trickNumber;
    m_turn = winner;
    m_phase = Phase::Playing;
    Q_EMIT changed();
    Q_EMIT turnBegan(m_turn);
}

// ---------------------------------------------------------------------------
// La marque.
//
//     (25 + écart + petit au bout) × multiplicateur + poignée + chelem
//
// L'écart est la différence entre ce que le preneur a ramassé et ce qu'il lui
// fallait ; il est pris en valeur absolue, le signe du résultat étant donné
// par la réussite ou la chute du contrat. Le petit au bout, la poignée et le
// chelem vont au camp qui gagne la donne, et non à celui qui les a faits :
// une poignée montrée par un défenseur qui chute est une poignée offerte au
// preneur.
//
// Le preneur étant seul contre trois, il encaisse ou paie trois fois ce que
// chaque défenseur paie ou encaisse. La somme des quatre lignes est nulle, et
// c'est ce qui permet de la vérifier d'un coup d'œil sur la feuille.

void Tarot::finishDonne() {
    m_phase = Phase::DonneOver;

    DonneResult done;
    done.played = true;
    done.contract = m_contract;
    done.taker = m_taker;
    for (int team = 0; team < kTeamCount; ++team) {
        done.points[team] = m_points[team] + m_extra[team];
        done.tricks[team] = m_tricks[team];
    }
    done.bouts = m_bouts[kTakerTeam] + m_extraBouts[kTakerTeam];
    done.target = targetFor(done.bouts);
    done.ecart = done.points[kTakerTeam] - done.target;
    done.made = done.ecart >= 0;

    const int winner = done.made ? kTakerTeam : kDefenceTeam;

    // Le petit au bout : dix points, du bon côté ou du mauvais. Il entre dans
    // la parenthèse, et il est donc multiplié par le contrat.
    int petit = 0;
    if (m_petitAuBoutTeam >= 0) {
        petit = m_petitAuBoutTeam == winner ? kPetitAuBout : -kPetitAuBout;
        done.petitAuBoutTeam = m_petitAuBoutTeam;
    }

    int bracket = kDonneBase + std::abs(done.ecart) + petit;
    if (bracket < 0) bracket = 0;
    int unit = bracket * contractMultiplier(m_contract);

    // Les poignées montrées, de part et d'autre : elles s'ajoutent hors
    // multiplicateur, et toujours au camp qui gagne la donne.
    for (int at = 0; at < kSeatCount; ++at) {
        const PoigneeKind kind = m_seats.at(at).poignee;
        if (kind == PoigneeKind::Aucune) continue;
        done.poigneePoints += poigneePoints(kind);
        if (done.poignee == PoigneeKind::Aucune) {
            done.poignee = kind;
            done.poigneeSeat = at;
        }
    }
    unit += done.poigneePoints;

    int signed_ = done.made ? unit : -unit;

    // Le chelem. Il ne dépend pas du contrat et ne se multiplie pas : c'est
    // une prime à part, qui s'ajoute une fois tout le reste compté.
    const bool takerSlam = m_tricks[kTakerTeam] == kTrickCount;
    const bool defenceSlam = m_tricks[kDefenceTeam] == kTrickCount;
    done.chelemAnnounced = m_chelemAnnounced;
    done.chelemMade = takerSlam;
    if (m_chelemAnnounced) {
        done.chelem = takerSlam ? kChelemAnnonceReussi : -kChelemAnnonceManque;
    } else if (takerSlam) {
        done.chelem = kChelemSansAnnonce;
    } else if (defenceSlam) {
        done.chelem = -kChelemSansAnnonce;
    }
    signed_ += done.chelem;

    done.unit = signed_;
    for (int at = 0; at < kSeatCount; ++at) {
        done.gained[at] = at == m_taker ? signed_ * (kSeatCount - 1) : -signed_;
        m_score[at] += done.gained[at];
    }

    m_result = done;
    ++m_donneNumber;

    Q_EMIT changed();
    Q_EMIT donneFinished();

    if (m_donneNumber < m_donnes) return;

    // La partie est finie : celui qui mène a gagné. Deux joueurs à égalité en
    // tête et la partie est nulle, ce qui arrive et qui se dit.
    int best = m_score[0];
    for (int at = 1; at < kSeatCount; ++at) best = std::max(best, m_score[at]);
    int leaders = 0;
    int which = -1;
    for (int at = 0; at < kSeatCount; ++at) {
        if (m_score[at] != best) continue;
        ++leaders;
        which = at;
    }
    m_matchWinner = leaders == 1 ? which : -1;

    m_phase = Phase::MatchOver;
    Q_EMIT matchFinished();
}
