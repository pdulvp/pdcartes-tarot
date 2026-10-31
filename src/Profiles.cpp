#include "Profiles.h"

#include <QCoreApplication>
#include <QHash>
#include <QSettings>

namespace {

int clampSlot(int slot) { return qBound(1, slot, kProfileCount); }

} // namespace

QString profileKey(int slot, const QString &suffix) {
    return QStringLiteral("profil/%1/%2").arg(clampSlot(slot)).arg(suffix);
}

int currentProfile() {
    QSettings settings;
    return clampSlot(settings.value(QStringLiteral("profil/courant"), 1).toInt());
}

void setCurrentProfile(int slot) {
    QSettings settings;
    settings.setValue(QStringLiteral("profil/courant"), clampSlot(slot));
}

QString profileName(int slot) {
    QSettings settings;
    const QString kept = settings.value(profileKey(slot, QStringLiteral("nom"))).toString();
    if (!kept.trimmed().isEmpty()) return kept.trimmed();
    return QCoreApplication::translate("profiles", "Profil %1").arg(clampSlot(slot));
}

void setProfileName(int slot, const QString &name) {
    QSettings settings;
    settings.setValue(profileKey(slot, QStringLiteral("nom")),
                      name.trimmed().left(kProfileNameLength));
}

ProfileRecord profileRecord(int slot) {
    QSettings settings;
    ProfileRecord record;
    record.slot = clampSlot(slot);
    record.name = profileName(slot);
    record.matchesPlayed = settings.value(profileKey(slot, QStringLiteral("parties")), 0).toInt();
    record.matchesWon = settings.value(profileKey(slot, QStringLiteral("gagnees")), 0).toInt();
    record.donnesPlayed = settings.value(profileKey(slot, QStringLiteral("donnes")), 0).toInt();
    record.donnesWon =
        settings.value(profileKey(slot, QStringLiteral("donnesGagnees")), 0).toInt();
    record.bestDonnes = settings.value(profileKey(slot, QStringLiteral("meilleur")), -1).toInt();
    return record;
}

void noteDonnePlayed(bool won) {
    const int slot = currentProfile();
    QSettings settings;
    settings.setValue(profileKey(slot, QStringLiteral("donnes")),
                      settings.value(profileKey(slot, QStringLiteral("donnes")), 0).toInt() + 1);
    if (!won) return;
    settings.setValue(
        profileKey(slot, QStringLiteral("donnesGagnees")),
        settings.value(profileKey(slot, QStringLiteral("donnesGagnees")), 0).toInt() + 1);
}

void noteMatchPlayed(bool won, int donnes) {
    const int slot = currentProfile();
    QSettings settings;
    settings.setValue(profileKey(slot, QStringLiteral("parties")),
                      settings.value(profileKey(slot, QStringLiteral("parties")), 0).toInt() + 1);
    if (!won) return;
    settings.setValue(profileKey(slot, QStringLiteral("gagnees")),
                      settings.value(profileKey(slot, QStringLiteral("gagnees")), 0).toInt() + 1);

    // Le moins de donnes qu'il a fallu pour gagner une partie, qui est le
    // seul record qu'il y ait à garder dans un jeu dont le but en points est
    // celui de la table qu'on a choisie.
    const int standing = settings.value(profileKey(slot, QStringLiteral("meilleur")), -1).toInt();
    if (donnes > 0 && (standing < 0 || donnes < standing)) {
        settings.setValue(profileKey(slot, QStringLiteral("meilleur")), donnes);
    }
}

void forgetProfile(int slot) {
    const int at = clampSlot(slot);
    QSettings settings;

    // Les noms reviennent après : celui du profil, et ceux qu'on a donnés aux
    // six. Effacer ses parties n'est pas débaptiser la table, et la question
    // posée avant d'arriver ici ne parle que de parties et de niveaux.
    const QString kept = settings.value(profileKey(at, QStringLiteral("nom"))).toString();

    // Relevés à la clé plutôt que demandés aux joueurs : les profils n'ont pas
    // à savoir qui sont les six.
    const QString branch = QStringLiteral("profil/%1/rival").arg(at);
    QHash<QString, QString> called;
    settings.beginGroup(branch);
    for (const QString &key : settings.allKeys()) {
        if (!key.endsWith(QLatin1String("/nom"))) continue;
        const QString name = settings.value(key).toString();
        if (!name.trimmed().isEmpty()) called.insert(key, name);
    }
    settings.endGroup();

    // L'expérience des six est gardée sous le profil elle aussi, et un
    // chiffre absent veut dire « le niveau avec lequel celui-là s'assoit » --
    // alors enlever toute la branche est précisément ce que remettre les six
    // comme ils étaient veut dire.
    settings.remove(QStringLiteral("profil/%1").arg(at));

    if (!kept.trimmed().isEmpty()) {
        settings.setValue(profileKey(at, QStringLiteral("nom")), kept.trimmed());
    }
    settings.beginGroup(branch);
    for (auto it = called.constBegin(); it != called.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
}
