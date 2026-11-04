#include "Poignee.h"

#include <QCoreApplication>

#include <algorithm>

int poigneeTrumps(PoigneeKind kind, int seats) {
    // Règlement FFT : 13/15/18 atouts à 3 joueurs, 10/13/15 à 4, 8/10/13 à 5.
    const int shift = seats <= 3 ? 3 : (seats >= 5 ? -2 : 0);
    switch (kind) {
    case PoigneeKind::Simple: return kSimpleTrumps + shift;
    case PoigneeKind::Double: return kDoubleTrumps + shift;
    case PoigneeKind::Triple: return kTripleTrumps + shift;
    case PoigneeKind::Aucune: break;
    }
    return 0;
}

int poigneePoints(PoigneeKind kind) {
    switch (kind) {
    case PoigneeKind::Simple: return kSimplePoints;
    case PoigneeKind::Double: return kDoublePoints;
    case PoigneeKind::Triple: return kTriplePoints;
    case PoigneeKind::Aucune: break;
    }
    return 0;
}

QString poigneeName(PoigneeKind kind) {
    switch (kind) {
    case PoigneeKind::Simple: return QCoreApplication::translate("poignee", "simple poignée");
    case PoigneeKind::Double: return QCoreApplication::translate("poignee", "double poignée");
    case PoigneeKind::Triple: return QCoreApplication::translate("poignee", "triple poignée");
    case PoigneeKind::Aucune: break;
    }
    return QString();
}

int trumpsInHand(const QVector<Card> &hand) {
    int found = 0;
    for (const Card &card : hand) {
        if (card.suit == Suit::Atout) ++found;
    }
    return found;
}

PoigneeKind bestPoignee(const QVector<Card> &hand, int seats) {
    const int all = trumpsInHand(hand);
    if (all >= poigneeTrumps(PoigneeKind::Triple, seats)) return PoigneeKind::Triple;
    if (all >= poigneeTrumps(PoigneeKind::Double, seats)) return PoigneeKind::Double;
    if (all >= poigneeTrumps(PoigneeKind::Simple, seats)) return PoigneeKind::Simple;
    return PoigneeKind::Aucune;
}

QVector<Card> poigneeCards(const QVector<Card> &hand, PoigneeKind kind, int seats) {
    const int wanted = poigneeTrumps(kind, seats);
    if (wanted <= 0) return {};

    QVector<Card> trumps;
    Card excuse;
    for (const Card &card : hand) {
        if (card.suit != Suit::Atout) continue;
        if (card.excuse()) {
            excuse = card;
            continue;
        }
        trumps.append(card);
    }
    // Les plus hauts d'abord : une poignée se pose du vingt et un vers le
    // petit, et c'est dans cet ordre qu'on la lit.
    std::sort(trumps.begin(), trumps.end(),
              [](const Card &a, const Card &b) { return cardStrength(a) > cardStrength(b); });

    QVector<Card> shown;
    const int fromTrumps = std::min(wanted, int(trumps.size()));
    for (int i = 0; i < fromTrumps; ++i) shown.append(trumps.at(i));
    // L'excuse ne vient qu'en dernier, et seulement s'il manque une carte pour
    // atteindre le compte : c'est la condition même de son emploi.
    if (shown.size() < wanted && excuse.valid()) shown.append(excuse);
    if (shown.size() < wanted) return {};
    return shown;
}
