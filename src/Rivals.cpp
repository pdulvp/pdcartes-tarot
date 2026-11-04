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
        {QStringLiteral("lulu"), QCoreApplication::translate("rivals", "Lulu"), 196, 0,
         4},
        {QStringLiteral("nestor"), QCoreApplication::translate("rivals", "Nestor"), 34, 70,
         3},
        {QStringLiteral("zephyr"), QCoreApplication::translate("rivals", "Zéphyr"), 268, 230,
         5},
        {QStringLiteral("prune"), QCoreApplication::translate("rivals", "Prune"), 328,
         330, 1},
        {QStringLiteral("barnabe"), QCoreApplication::translate("rivals", "Barnabé"), 140, 440, 2},
        {QStringLiteral("oriane"), QCoreApplication::translate("rivals", "Oriane"), 352,
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

Innates rivalInnateAsArrived(const QString &rivalId) {
    const quint32 h = hashOf(rivalById(rivalId).id);
    Innates innates;
    for (int i = 0; i < kInnateCount; ++i) {
        innates.level[i] = int((h >> (8 * i)) & 0xFF) % (kInnateTop + 1);
    }
    return innates;
}

Innates rivalInnate(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    const Innates arrived = rivalInnateAsArrived(who.id);
    QSettings settings;
    Innates innates = arrived;
    for (int i = 0; i < kInnateCount; ++i) {
        innates.level[i] =
            settings.value(rivalKey(currentProfile(), who.id, QStringLiteral("innee%1").arg(i)),
                           arrived.level[i])
                .toInt();
    }
    for (int i = 0; i < kInnateCount; ++i) {
        innates.level[i] = qBound(0, innates.level[i], kInnateTop);
    }
    return innates;
}

void setRivalInnate(const QString &rivalId, const Innates &innates) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    // Aucun changement effectif, aucune perte : ouvrir puis refermer la fiche
    // ne doit pas entraîner la perte de l'expérience.
    if (rivalInnate(who.id) == innates) return;

    QSettings settings;
    for (int i = 0; i < kInnateCount; ++i) {
        settings.setValue(rivalKey(slot, who.id, QStringLiteral("innee%1").arg(i)),
                          innates.level[i]);
    }
    settings.sync();
    // L'expérience acquise est annulée, ayant été obtenue avec un autre
    // tempérament.
    resetRivalExperience(who.id);
}

void resetRivalExperience(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    QSettings settings;
    // Sont remis à zéro : xp, les quatre pools et le palmarès. Le nom et le
    // tempérament ne relèvent pas de l'expérience.
    settings.remove(rivalKey(slot, who.id, QStringLiteral("xp")));
    for (int i = 0; i < kLearnedCount; ++i) {
        settings.remove(rivalKey(slot, who.id, QStringLiteral("pool%1").arg(i)));
    }
    settings.remove(rivalKey(slot, who.id, QStringLiteral("donnes")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("donnesGagnees")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("contrats")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("parties")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("partiesGagnees")));
}

int rungFloor(int rung) {
    constexpr int kFloors[kRungCount] = {0, 50, 130, 220};
    return kFloors[qBound(0, rung, kRungCount - 1)];
}

int rungOf(int poolXp) {
    const int xp = qBound(0, poolXp, kPoolCeiling);
    int rung = 0;
    for (int i = 1; i < kRungCount; ++i) {
        if (xp >= rungFloor(i)) rung = i;
    }
    return rung;
}

int rungGap(int poolXp) {
    const int rung = rungOf(poolXp);
    if (rung >= kRungCount - 1) return 0;
    return rungFloor(rung + 1) - qBound(0, poolXp, kPoolCeiling);
}

qreal rungProgress(int poolXp) {
    const int rung = rungOf(poolXp);
    const int bottom = rungFloor(rung);
    const int top = rung < kRungCount - 1 ? rungFloor(rung + 1) : kPoolCeiling;
    if (top <= bottom) return 1.0;
    return qBound(0.0, qreal(qBound(bottom, poolXp, top) - bottom) / (top - bottom), 1.0);
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

const QVector<InnateSpec> &innateSpecs() {
    static const QVector<InnateSpec> list = {
        {QCoreApplication::translate("rivals", "Audace"), 14,
         {QCoreApplication::translate("rivals", "timide"),
          QCoreApplication::translate("rivals", "mesuré"),
          QCoreApplication::translate("rivals", "hardi"),
          QCoreApplication::translate("rivals", "casse-cou")},
         {QCoreApplication::translate("rivals", "laisse passer les mains moyennes"),
          QCoreApplication::translate("rivals", "prend quand ça vaut le coup"),
          QCoreApplication::translate("rivals", "prend tôt, et assume"),
          QCoreApplication::translate("rivals", "prend presque tout, même risqué")}},
        {QCoreApplication::translate("rivals", "Prudence"), 42,
         {QCoreApplication::translate("rivals", "insouciant"),
          QCoreApplication::translate("rivals", "ordinaire"),
          QCoreApplication::translate("rivals", "regardant"),
          QCoreApplication::translate("rivals", "économe")},
         {QCoreApplication::translate("rivals", "se débarrasse de ce qui coûte"),
          QCoreApplication::translate("rivals", "garde ce qui sert"),
          QCoreApplication::translate("rivals", "garde ses bouts et son petit"),
          QCoreApplication::translate("rivals", "ne lâche aucune carte utile")}},
        {QCoreApplication::translate("rivals", "Vigilance"), 320,
         {QCoreApplication::translate("rivals", "distrait"),
          QCoreApplication::translate("rivals", "ordinaire"),
          QCoreApplication::translate("rivals", "attentif"),
          QCoreApplication::translate("rivals", "aux aguets")},
         {QCoreApplication::translate("rivals", "ne regarde pas qui tient le pli"),
          QCoreApplication::translate("rivals", "suit le jeu de temps en temps"),
          QCoreApplication::translate("rivals", "sait qui tient le pli, et de quel camp"),
          QCoreApplication::translate("rivals", "devine l'appelé caché à qui donne au preneur")}},
        {QCoreApplication::translate("rivals", "Calme"), 92,
         {QCoreApplication::translate("rivals", "soupe au lait"),
          QCoreApplication::translate("rivals", "ordinaire"),
          QCoreApplication::translate("rivals", "posé"),
          QCoreApplication::translate("rivals", "imperturbable")},
         {QCoreApplication::translate("rivals", "se trompe un coup sur trois"),
          QCoreApplication::translate("rivals", "se trompe de temps en temps"),
          QCoreApplication::translate("rivals", "ne se trompe presque plus"),
          QCoreApplication::translate("rivals", "ne se trompe jamais")}},
    };
    return list;
}

const QVector<LearnedSpec> &learnedSpecs() {
    static const QVector<LearnedSpec> list = {
        {QCoreApplication::translate("rivals", "Lecture"), 206,
         {QCoreApplication::translate("rivals", "ne compte que ses atouts"),
          QCoreApplication::translate("rivals", "pèse bouts et atouts"),
          QCoreApplication::translate("rivals", "pèse toute la main"),
          QCoreApplication::translate("rivals", "pèse juste, à une carte près")},
         {QCoreApplication::translate("rivals", "prend sur n'importe quelle main"),
          QCoreApplication::translate("rivals", "prend sur ce qu'il a compté"),
          QCoreApplication::translate("rivals", "prend ce qui tient vraiment"),
          QCoreApplication::translate("rivals", "ne prend que ce qui gagne")}},
        {QCoreApplication::translate("rivals", "Mémoire"), 268,
         {QCoreApplication::translate("rivals", "oublie tout ce qui est joué"),
          QCoreApplication::translate("rivals", "se souvient du dernier pli"),
          QCoreApplication::translate("rivals", "sait qui tient le pli"),
          QCoreApplication::translate("rivals", "sait ce qui reste dehors")},
         {QCoreApplication::translate("rivals", "joue sans regarder le tapis"),
          QCoreApplication::translate("rivals", "suit le pli de temps en temps"),
          QCoreApplication::translate("rivals", "charge le pli de son camp"),
          QCoreApplication::translate("rivals", "compte les soixante-dix-huit cartes")}},
        {QCoreApplication::translate("rivals", "Défausse"), 174,
         {QCoreApplication::translate("rivals", "jette les moins chères"),
          QCoreApplication::translate("rivals", "garde les bouts"),
          QCoreApplication::translate("rivals", "vide une couleur entière"),
          QCoreApplication::translate("rivals", "se fait la coupe qu'il faut")},
         {QCoreApplication::translate("rivals", "reste long partout, sans coupe"),
          QCoreApplication::translate("rivals", "garde ce qui peut prendre"),
          QCoreApplication::translate("rivals", "se fait une coupe sûre"),
          QCoreApplication::translate("rivals", "coupe exactement où il faudra")}},
        {QCoreApplication::translate("rivals", "Maîtrise"), 146,
         {QCoreApplication::translate("rivals", "bourde un coup sur trois"),
          QCoreApplication::translate("rivals", "bourde rarement"),
          QCoreApplication::translate("rivals", "protège son petit"),
          QCoreApplication::translate("rivals", "ne se trompe plus")},
         {QCoreApplication::translate("rivals", "joue la mauvaise carte souvent"),
          QCoreApplication::translate("rivals", "garde la tête froide"),
          QCoreApplication::translate("rivals", "pose son petit sur un pli acquis"),
          QCoreApplication::translate("rivals", "joue chaque carte au mieux")}},
    };
    return list;
}

const InnateSpec &innateSpec(Innate which) {
    return innateSpecs().at(qBound(0, int(which), kInnateCount - 1));
}

const LearnedSpec &learnedSpec(Learned which) {
    return learnedSpecs().at(qBound(0, int(which), kLearnedCount - 1));
}

QString rungRoman(int rung) {
    switch (qBound(0, rung, kRungCount - 1)) {
    case 0: return QStringLiteral("I");
    case 1: return QStringLiteral("II");
    case 2: return QStringLiteral("III");
    default: return QStringLiteral("IV");
    }
}

QString temperamentLine(const Innates &innates) {
    // L'innée la plus marquée fonde la phrase.
    int dominant = 0;
    for (int i = 1; i < kInnateCount; ++i) {
        if (innates.level[i] > innates.level[dominant]) dominant = i;
    }
    const Innate which = Innate(dominant);
    return QCoreApplication::translate("rivals", "Plutôt %1")
        .arg(innateSpec(which).levels[innates.level[dominant]].toLower());
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
        learning.pools = poolsFromXp(learning.xp, rivalInnate(who.id));
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
