#include "Cards.h"

#include "CardArt.h"

#include <QCoreApplication>
#include <QLocale>
#include <QRandomGenerator>

#include <algorithm>

QVector<Card> freshPack() {
    QVector<Card> pack;
    pack.reserve(kPackSize);
    for (int suit = 0; suit < kSuitCount; ++suit) {
        for (int rank = kAs; rank <= kRoi; ++rank) {
            Card card;
            card.rank = rank;
            card.suit = Suit(suit);
            card.id = cardIdFor(rank, card.suit);
            pack.append(card);
        }
    }
    for (int number = 1; number <= kTrumpCount; ++number) {
        Card card;
        card.rank = number;
        card.suit = Suit::Atout;
        card.id = cardIdFor(number, Suit::Atout);
        pack.append(card);
    }
    Card excuse;
    excuse.rank = kExcuse;
    excuse.suit = Suit::Atout;
    excuse.id = kExcuseId;
    pack.append(excuse);
    return pack;
}

void shuffleCards(QVector<Card> &cards) {
    // Fisher-Yates, tiré du générateur de Qt plutôt que de std::shuffle : une
    // seule source de hasard dans le programme, et c'est celle d'où sortent la
    // donne et les hésitations des adversaires.
    for (int i = cards.size() - 1; i > 0; --i) {
        const int j = QRandomGenerator::global()->bounded(i + 1);
        std::swap(cards[i], cards[j]);
    }
}

int targetFor(int bouts) { return kTargets[std::clamp(bouts, 0, kBoutCount)]; }

bool isBout(const Card &card) {
    if (!card.valid() || card.suit != Suit::Atout) return false;
    return card.rank == kExcuse || card.rank == kPetit || card.rank == kVingtEtUn;
}

bool isRed(Suit suit) {
    if (suit == Suit::Atout) return false;
    return cardSuitIsRed(int(suit));
}

QString suitPip(Suit suit) {
    switch (suit) {
    case Suit::Pique: return QStringLiteral("♠");
    case Suit::Coeur: return QStringLiteral("♥");
    case Suit::Carreau: return QStringLiteral("♦");
    case Suit::Trefle: return QStringLiteral("♣");
    // L'atout n'a pas de symbole imprimé : il porte un numéro. L'étoile tient
    // lieu d'enseigne là où la place manque pour écrire le mot.
    case Suit::Atout: return QStringLiteral("✦");
    }
    return QString();
}

QString suitName(Suit suit) {
    switch (suit) {
    case Suit::Pique: return QCoreApplication::translate("cards", "pique");
    case Suit::Coeur: return QCoreApplication::translate("cards", "cœur");
    case Suit::Carreau: return QCoreApplication::translate("cards", "carreau");
    case Suit::Trefle: return QCoreApplication::translate("cards", "trèfle");
    case Suit::Atout: return QCoreApplication::translate("cards", "atout");
    }
    return QString();
}

QString rankLabel(int rank, Suit suit) {
    if (suit == Suit::Atout) {
        if (rank == kExcuse) return QCoreApplication::translate("cards", "Exc");
        return QString::number(rank);
    }
    // Les lettres des figures viennent de la cartothèque, qui les imprime sur
    // les cartes : deux tables finiraient par diverger. Le cavalier y porte le
    // rang 14, le roi le rang 13 ; ici c'est l'inverse, parce que le roi prend
    // le pli au cavalier. La traduction est faite par `libraryRank()`.
    return cardRankLabel(rank == kCavalier   ? 14
                         : rank == kDame     ? 12
                         : rank == kRoi      ? 13
                                             : rank);
}

QString cardLabel(const Card &card) {
    if (!card.valid()) return QString();
    if (card.excuse()) return rankLabel(card.rank, card.suit);
    return rankLabel(card.rank, card.suit) + suitPip(card.suit);
}

QString cardName(const Card &card) {
    if (!card.valid()) return QString();

    if (card.suit == Suit::Atout) {
        if (card.rank == kExcuse) return QCoreApplication::translate("cards", "l'excuse");
        if (card.rank == kPetit) return QCoreApplication::translate("cards", "le petit");
        return QCoreApplication::translate("cards", "l'atout %1").arg(card.rank);
    }

    QString head;
    switch (card.rank) {
    case kAs: head = QCoreApplication::translate("cards", "l'as"); break;
    case kRoi: head = QCoreApplication::translate("cards", "le roi"); break;
    case kDame: head = QCoreApplication::translate("cards", "la dame"); break;
    case kCavalier: head = QCoreApplication::translate("cards", "le cavalier"); break;
    case kValet: head = QCoreApplication::translate("cards", "le valet"); break;
    default: head = QCoreApplication::translate("cards", "le %1").arg(card.rank); break;
    }
    return QCoreApplication::translate("cards", "%1 de %2").arg(head, suitName(card.suit));
}

QString pointsText(int halves) {
    // Les demi-points sont comptés en entiers ; ils se lisent en points, avec
    // la virgule décimale du français quand le compte est impair.
    const QString whole = QLocale().toString(halves / 2);
    if (halves % 2 == 0) return whole;
    return QCoreApplication::translate("cards", "%1,5").arg(whole);
}

int cardPoints(const Card &card) {
    if (!card.valid()) return 0;
    if (isBout(card)) return 9;                 // 4,5 points
    if (card.suit == Suit::Atout) return 1;     // 0,5 point
    switch (card.rank) {
    case kRoi: return 9;                        // 4,5
    case kDame: return 7;                       // 3,5
    case kCavalier: return 5;                   // 2,5
    case kValet: return 3;                      // 1,5
    default: return 1;                          // 0,5, de l'as au dix
    }
}

int cardStrength(const Card &card) {
    if (!card.valid()) return 0;
    // L'excuse ne prend rien : sa force est nulle, et c'est ce qui la fait
    // perdre toute comparaison sans qu'on ait à l'écrire ailleurs.
    if (card.excuse()) return 0;
    // Les vingt et un atouts passent devant les quatorze rangs des enseignes.
    if (card.suit == Suit::Atout) return kRanksInSuit + card.rank;
    return card.rank;
}

bool beatsTrick(const Card &candidate, const Card &best, Suit led) {
    if (!candidate.valid()) return false;
    // L'excuse est jouée mais ne dispute rien : elle ne prend pas le pli, même
    // posée la première sur une table vide.
    if (candidate.excuse()) return false;
    if (!best.valid() || best.excuse()) return true;

    const bool mineTrump = candidate.suit == Suit::Atout;
    const bool bestTrump = best.suit == Suit::Atout;
    if (mineTrump != bestTrump) return mineTrump;            // l'atout coupe
    if (mineTrump) return cardStrength(candidate) > cardStrength(best);

    // Hors atout, une carte qui n'est pas de l'enseigne demandée ne prend
    // rien : c'est une défausse, et elle tombe sous la plus petite des cartes
    // fournies.
    if (candidate.suit != led) return false;
    if (best.suit != led) return true;
    return cardStrength(candidate) > cardStrength(best);
}

namespace {

// Les atouts à gauche de la main, les enseignes derrière dans l'ordre
// habituel : une main de tarot se tient ainsi, parce que l'atout est la
// famille dont on compte les cartes pendant toute la donne.
int suitOrder(Suit suit) { return suit == Suit::Atout ? -1 : int(suit); }

} // namespace

void sortBySuit(QVector<Card> &cards) {
    std::stable_sort(cards.begin(), cards.end(), [](const Card &a, const Card &b) {
        const int sa = suitOrder(a.suit);
        const int sb = suitOrder(b.suit);
        if (sa != sb) return sa < sb;
        // La plus forte en tête : le vingt et un à gauche des atouts, le roi
        // en tête de son enseigne. L'excuse, dont la force est nulle, se range
        // donc au bout des atouts, où un joueur la tient.
        return cardStrength(a) > cardStrength(b);
    });
}

void sortByValue(QVector<Card> &cards) {
    std::stable_sort(cards.begin(), cards.end(), [](const Card &a, const Card &b) {
        const int pa = cardPoints(a);
        const int pb = cardPoints(b);
        if (pa != pb) return pa > pb;
        const int sa = suitOrder(a.suit);
        const int sb = suitOrder(b.suit);
        if (sa != sb) return sa < sb;
        return cardStrength(a) > cardStrength(b);
    });
}

Card takeCard(QVector<Card> &cards, int id) {
    for (int i = 0; i < cards.size(); ++i) {
        if (cards.at(i).id != id) continue;
        const Card found = cards.at(i);
        cards.remove(i);
        return found;
    }
    return Card{};
}

const Card *findCard(const QVector<Card> &cards, int id) {
    for (const Card &card : cards) {
        if (card.id == id) return &card;
    }
    return nullptr;
}

bool hasSuit(const QVector<Card> &cards, Suit suit) {
    for (const Card &card : cards) {
        // L'excuse est rangée avec les atouts mais n'en est pas un : une main
        // qui n'a qu'elle est une main sans atout, et son détenteur est libre
        // de se défausser.
        if (card.suit == suit && !card.excuse()) return true;
    }
    return false;
}

int countSuit(const QVector<Card> &cards, Suit suit) {
    int found = 0;
    for (const Card &card : cards) {
        if (card.suit == suit && !card.excuse()) ++found;
    }
    return found;
}

int totalPoints(const QVector<Card> &cards) {
    int sum = 0;
    for (const Card &card : cards) sum += cardPoints(card);
    return sum;
}

int countBouts(const QVector<Card> &cards) {
    int found = 0;
    for (const Card &card : cards) {
        if (isBout(card)) ++found;
    }
    return found;
}
