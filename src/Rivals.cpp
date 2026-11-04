#include "Rivals.h"

#include "Profiles.h"

#include <QCoreApplication>
#include <QSettings>

#include <algorithm>

namespace {

const QVector<Rival> &builtRivals() {
    // Six pour trois places, de sorte qu'il y ait un choix à faire et que les
    // six ne jouent pas tous à chaque partie. Les niveaux de départ sont
    // écartés exprès : la première table à laquelle on s'assoit doit contenir
    // quelqu'un qu'on peut battre et quelqu'un qu'on ne peut pas.
    static const QVector<Rival> list = {
        {QStringLiteral("lulu"), QCoreApplication::translate("rivals", "Lulu"),
         QCoreApplication::translate("rivals", "Prend sur cinq atouts, et le regrette"), 196, 0,
         4},
        {QStringLiteral("nestor"), QCoreApplication::translate("rivals", "Nestor"),
         QCoreApplication::translate("rivals", "Garde ses rois jusqu'au dernier pli"), 34, 70,
         3},
        {QStringLiteral("zephyr"), QCoreApplication::translate("rivals", "Zéphyr"),
         QCoreApplication::translate("rivals", "Charge dès que le pli est à sa défense"), 268, 230,
         5},
        {QStringLiteral("prune"), QCoreApplication::translate("rivals", "Prune"),
         QCoreApplication::translate("rivals", "Pèse sa main, et passe les mains moyennes"), 328,
         330, 1},
        {QStringLiteral("barnabe"), QCoreApplication::translate("rivals", "Barnabé"),
         QCoreApplication::translate("rivals", "Tire l'atout dès qu'il a pris"), 140, 440, 2},
        {QStringLiteral("oriane"), QCoreApplication::translate("rivals", "Oriane"),
         QCoreApplication::translate("rivals", "Sait ce qui reste dehors, et depuis quand"), 352,
         660, 0},
    };
    return list;
}

QString rivalKey(int slot, const QString &rivalId, const QString &what) {
    return profileKey(slot, QStringLiteral("rival/%1/%2").arg(rivalId, what));
}

// Hash FNV-1a sur l'id : stable d'un lancement à l'autre, contrairement à
// qHash(QString) dont la graine est tirée au hasard à chaque démarrage.
quint32 hashOf(const QString &id) {
    quint32 h = 2166136261u;
    for (const QChar c : id) {
        h ^= quint32(c.unicode());
        h *= 16777619u;
    }
    return h;
}

// L'innée jumelée à chaque acquise : c'est elle qui en accélère
// l'apprentissage.
Innate feeds(Learned which) {
    switch (which) {
    case Learned::Lecture: return Innate::Audace;
    case Learned::Memoire: return Innate::Vigilance;
    case Learned::Defausse: return Innate::Prudence;
    case Learned::Maitrise: return Innate::Calme;
    }
    return Innate::Audace;
}

} // namespace

bool Innates::operator==(const Innates &other) const {
    for (int i = 0; i < kInnateCount; ++i) {
        if (level[i] != other.level[i]) return false;
    }
    return true;
}

int Pools::total() const {
    int sum = 0;
    for (int i = 0; i < kLearnedCount; ++i) sum += xp[i];
    return sum;
}

Innates innatesFor(const QString &rivalId) {
    const quint32 h = hashOf(rivalById(rivalId).id);
    Innates innates;
    for (int i = 0; i < kInnateCount; ++i) {
        innates.level[i] = int((h >> (8 * i)) & 0xFF) % (kInnateTop + 1);
    }
    return innates;
}

int rungOf(int poolXp) {
    constexpr int kFloors[kRungCount] = {0, 50, 130, 220};
    int rung = 0;
    for (int i = 1; i < kRungCount; ++i) {
        if (poolXp >= kFloors[i]) rung = i;
    }
    return rung;
}

qreal depthOf(int poolXp) {
    return qBound(0.0, qreal(qBound(0, poolXp, kPoolCeiling)) / kPoolCeiling, 1.0);
}

qreal learnRate(const Innates &innates, Learned which) {
    return 0.75 + 0.25 * innates.at(feeds(which));
}

Pools poolsFromXp(int xp, const Innates &innates) {
    Pools pools;
    double weight[kLearnedCount];
    double sum = 0.0;
    for (int i = 0; i < kLearnedCount; ++i) {
        weight[i] = learnRate(innates, Learned(i));
        sum += weight[i];
    }
    int assigned = 0;
    for (int i = 0; i < kLearnedCount; ++i) {
        pools.xp[i] = qBound(0, int(xp * weight[i] / sum), kPoolCeiling);
        assigned += pools.xp[i];
    }
    // Le reste de la répartition va aux acquises les plus favorisées par le
    // tempérament, tant qu'elles ont de la place.
    while (assigned < xp) {
        int best = -1;
        for (int i = 0; i < kLearnedCount; ++i) {
            if (pools.xp[i] >= kPoolCeiling) continue;
            if (best < 0 || weight[i] > weight[best]) best = i;
        }
        if (best < 0) break;
        pools.xp[best] += 1;
        ++assigned;
    }
    return pools;
}

QString innateName(Innate which) {
    switch (which) {
    case Innate::Audace: return QCoreApplication::translate("rivals", "Audace");
    case Innate::Prudence: return QCoreApplication::translate("rivals", "Prudence");
    case Innate::Vigilance: return QCoreApplication::translate("rivals", "Vigilance");
    case Innate::Calme: return QCoreApplication::translate("rivals", "Calme");
    }
    return QString();
}

QString innateLevelName(Innate which, int level) {
    static const char *kNames[kInnateCount][kInnateTop + 1] = {
        {QT_TRANSLATE_NOOP("rivals", "timide"), QT_TRANSLATE_NOOP("rivals", "mesuré"),
         QT_TRANSLATE_NOOP("rivals", "hardi"), QT_TRANSLATE_NOOP("rivals", "casse-cou")},
        {QT_TRANSLATE_NOOP("rivals", "insouciant"), QT_TRANSLATE_NOOP("rivals", "ordinaire"),
         QT_TRANSLATE_NOOP("rivals", "regardant"), QT_TRANSLATE_NOOP("rivals", "économe")},
        {QT_TRANSLATE_NOOP("rivals", "distrait"), QT_TRANSLATE_NOOP("rivals", "ordinaire"),
         QT_TRANSLATE_NOOP("rivals", "attentif"), QT_TRANSLATE_NOOP("rivals", "aux aguets")},
        {QT_TRANSLATE_NOOP("rivals", "soupe au lait"), QT_TRANSLATE_NOOP("rivals", "ordinaire"),
         QT_TRANSLATE_NOOP("rivals", "posé"), QT_TRANSLATE_NOOP("rivals", "imperturbable")},
    };
    return QCoreApplication::translate("rivals", kNames[int(which)][qBound(0, level, kInnateTop)]);
}

QString learnedName(Learned which) {
    switch (which) {
    case Learned::Lecture: return QCoreApplication::translate("rivals", "Lecture");
    case Learned::Memoire: return QCoreApplication::translate("rivals", "Mémoire");
    case Learned::Defausse: return QCoreApplication::translate("rivals", "Défausse");
    case Learned::Maitrise: return QCoreApplication::translate("rivals", "Maîtrise");
    }
    return QString();
}

QString rungName(Learned which, int rung) {
    static const char *kRoman[kRungCount] = {"I", "II", "III", "IV"};
    return QStringLiteral("%1 %2")
        .arg(learnedName(which), QLatin1String(kRoman[qBound(0, rung, kRungCount - 1)]));
}

QString temperamentLine(const Innates &innates) {
    // L'innée la plus marquée fonde la phrase.
    int dominant = 0;
    for (int i = 1; i < kInnateCount; ++i) {
        if (innates.level[i] > innates.level[dominant]) dominant = i;
    }
    return QCoreApplication::translate("rivals", "Plutôt %1")
        .arg(innateLevelName(Innate(dominant), innates.level[dominant]).toLower());
}


const QVector<Rival> &rivals() { return builtRivals(); }

const Rival &rivalById(const QString &id) {
    for (const Rival &rival : builtRivals()) {
        if (rival.id == id) return rival;
    }
    return builtRivals().first();
}

Learning learningFor(const QString &rivalId, int slot) {
    const Rival &who = rivalById(rivalId);
    QSettings settings;
    Learning learning;
    // Pas de chiffre écrit veut dire que celui-là n'a pas encore joué sous ce
    // profil, et ce qu'il sait alors est ce avec quoi il s'est assis.
    learning.xp = settings.value(rivalKey(slot, who.id, QStringLiteral("xp")), who.startXp).toInt();
    learning.xp = qBound(0, learning.xp, kMaxExperience);
    // Les pools sont réparties par les actions des donnes depuis qu'elles
    // existent ; un profil écrit avant ça n'en a pas, et son xp est alors
    // réparti selon le tempérament de l'adversaire.
    if (settings.contains(rivalKey(slot, who.id, QStringLiteral("pool0")))) {
        for (int i = 0; i < kLearnedCount; ++i) {
            learning.pools.xp[i] =
                settings.value(rivalKey(slot, who.id, QStringLiteral("pool%1").arg(i)), 0)
                    .toInt();
        }
    } else {
        learning.pools = poolsFromXp(learning.xp, innatesFor(who.id));
    }
    learning.donnesPlayed =
        settings.value(rivalKey(slot, who.id, QStringLiteral("donnes")), 0).toInt();
    learning.donnesWon =
        settings.value(rivalKey(slot, who.id, QStringLiteral("donnesGagnees")), 0).toInt();
    learning.contractsMade =
        settings.value(rivalKey(slot, who.id, QStringLiteral("contrats")), 0).toInt();
    learning.matchesPlayed =
        settings.value(rivalKey(slot, who.id, QStringLiteral("parties")), 0).toInt();
    learning.matchesWon =
        settings.value(rivalKey(slot, who.id, QStringLiteral("partiesGagnees")), 0).toInt();
    return learning;
}

Learning learningFor(const QString &rivalId) {
    return learningFor(rivalId, currentProfile());
}

QString rivalName(const QString &rivalId, int slot) {
    const Rival &who = rivalById(rivalId);
    const QString called =
        QSettings().value(rivalKey(slot, who.id, QStringLiteral("nom"))).toString().trimmed();
    return called.isEmpty() ? who.name : called;
}

QString rivalName(const QString &rivalId) { return rivalName(rivalId, currentProfile()); }

void setRivalName(const QString &rivalId, const QString &name) {
    const Rival &who = rivalById(rivalId);
    const QString called = name.trimmed().left(kRivalNameLength);
    // Lui rendre son nom d'origine, c'est n'en avoir donné aucun : on efface
    // plutôt que d'écrire la même chose, pour que « a-t-il été rebaptisé » ait
    // une réponse et une seule.
    if (called.isEmpty() || called == who.name) {
        clearRivalName(rivalId);
        return;
    }
    QSettings().setValue(rivalKey(currentProfile(), who.id, QStringLiteral("nom")), called);
}

void clearRivalName(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    QSettings().remove(rivalKey(currentProfile(), who.id, QStringLiteral("nom")));
}

void noteRivalDonne(const QString &rivalId, bool teamWon, bool wasTaker, bool madeContract) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    Learning learning = learningFor(who.id, slot);

    // Une donne jouée vaut un peu quoi qu'il y soit arrivé -- du temps à table
    // est du temps à table -- et l'avoir gagnée vaut plus. La courbe que ça
    // fait est douce exprès : un débutant doit mettre trois ou quatre parties
    // à cesser d'en être un, et non une donne.
    int gained = 5;
    if (teamWon) gained += 7;
    if (wasTaker && madeContract) gained += 5;

    // Le même gain est réparti entre les quatre acquises selon ce que la
    // donne a demandé : lire sa main, suivre le jeu, faire son écart, tenir
    // son contrat.
    int deed[kLearnedCount] = {0, 0, 0, 0};
    deed[int(Learned::Lecture)] += 1;
    deed[int(Learned::Memoire)] += teamWon ? 2 : 1;
    if (wasTaker) {
        deed[int(Learned::Defausse)] += 1;
        deed[int(Learned::Maitrise)] += madeContract ? 3 : 1;
    }
    int deedSum = 0;
    for (int i = 0; i < kLearnedCount; ++i) deedSum += deed[i];
    int shared = 0;
    for (int i = 0; i < kLearnedCount; ++i) {
        const int part = deedSum > 0 ? gained * deed[i] / deedSum : 0;
        learning.pools.xp[i] = qBound(0, learning.pools.xp[i] + part, kPoolCeiling);
        shared += part;
    }
    // Le reste de la répartition va à la première acquise sollicitée.
    for (int i = 0; shared < gained && i < kLearnedCount; ++i) {
        if (deed[i] == 0 || learning.pools.xp[i] >= kPoolCeiling) continue;
        learning.pools.xp[i] += 1;
        ++shared;
    }

    learning.xp = qBound(0, learning.xp + gained, kMaxExperience);
    learning.donnesPlayed += 1;
    if (teamWon) learning.donnesWon += 1;
    if (wasTaker && madeContract) learning.contractsMade += 1;

    QSettings settings;
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("xp")), learning.xp);
    for (int i = 0; i < kLearnedCount; ++i) {
        settings.setValue(rivalKey(slot, who.id, QStringLiteral("pool%1").arg(i)),
                          learning.pools.xp[i]);
    }
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("donnes")), learning.donnesPlayed);
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("donnesGagnees")),
                      learning.donnesWon);
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("contrats")), learning.contractsMade);
}

void noteRivalMatch(const QString &rivalId, bool won) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    Learning learning = learningFor(who.id, slot);
    learning.matchesPlayed += 1;
    if (won) learning.matchesWon += 1;

    QSettings settings;
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("parties")), learning.matchesPlayed);
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("partiesGagnees")),
                      learning.matchesWon);
    // Gagner une partie entière vaut un peu plus que gagner une donne, et
    // c'est le seul endroit où le chiffre bouge en dehors d'une donne. Là,
    // tout ce qu'il a appris a un peu servi : le gain est réparti également.
    if (!won) return;
    learning.xp = qBound(0, learning.xp + 12, kMaxExperience);
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("xp")), learning.xp);
    for (int i = 0; i < kLearnedCount; ++i) {
        learning.pools.xp[i] = qBound(0, learning.pools.xp[i] + 3, kPoolCeiling);
        settings.setValue(rivalKey(slot, who.id, QStringLiteral("pool%1").arg(i)),
                          learning.pools.xp[i]);
    }
}

void resetRival(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    QSettings settings;
    settings.remove(QStringLiteral("profil/%1/rival/%2").arg(slot).arg(who.id));
}

Level levelOf(int xp) {
    if (xp >= 600) return Level::Expert;
    if (xp >= 200) return Level::Intermediaire;
    return Level::Debutant;
}

QString levelName(Level level) {
    switch (level) {
    case Level::Debutant: return QCoreApplication::translate("rivals", "Débutant");
    case Level::Intermediaire: return QCoreApplication::translate("rivals", "Intermédiaire");
    case Level::Expert: return QCoreApplication::translate("rivals", "Expert");
    }
    return QString();
}

int levelStars(Level level) {
    switch (level) {
    case Level::Debutant: return 1;
    case Level::Intermediaire: return 2;
    case Level::Expert: return 3;
    }
    return 1;
}

int levelFloor(Level level) {
    switch (level) {
    case Level::Debutant: return 0;
    case Level::Intermediaire: return 200;
    case Level::Expert: return 600;
    }
    return 0;
}

int levelCeiling(Level level) {
    switch (level) {
    case Level::Debutant: return 200;
    case Level::Intermediaire: return 600;
    case Level::Expert: return kMaxExperience;
    }
    return kMaxExperience;
}

qreal masteryOf(int xp) {
    return qBound(0.0, qreal(qBound(0, xp, kMaxExperience)) / kMaxExperience, 1.0);
}

qreal progressWithin(int xp) {
    const Level level = levelOf(xp);
    const int bottom = levelFloor(level);
    const int top = levelCeiling(level);
    if (top <= bottom) return 1.0;
    return qBound(0.0, qreal(qBound(bottom, xp, top) - bottom) / (top - bottom), 1.0);
}
