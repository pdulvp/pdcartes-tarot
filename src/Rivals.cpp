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

} // namespace

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

    learning.xp = qBound(0, learning.xp + gained, kMaxExperience);
    learning.donnesPlayed += 1;
    if (teamWon) learning.donnesWon += 1;
    if (wasTaker && madeContract) learning.contractsMade += 1;

    QSettings settings;
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("xp")), learning.xp);
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
    // c'est le seul endroit où le chiffre bouge en dehors d'une donne.
    if (!won) return;
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("xp")),
                      qBound(0, learning.xp + 12, kMaxExperience));
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
