#pragma once

#include <QString>

// Ce que c'est, et quand ça a été fait.
//
// La version est celle du fichier VERSION à la racine des sources -- trois
// nombres, avancés à la main quand quelque chose mérite d'être appelé une
// nouvelle version. Le build est le moment où le binaire a été compilé, en
// aammjjhhmmss : deux builds ne portent jamais le même, et c'est ce qui
// distingue le 1.0.0 d'hier de celui qui est devant vous.
//
// Un binaire de version ne porte pas d'empreinte du tout -- sa version est
// son nom, et il n'y en a qu'un. Le build est donc vide là, et ce qui le lit
// doit le prévoir.
//
// Les deux sont définis dans un fichier écrit à chaque build ; voir
// cmake/GenerateVersion.cmake.
extern const char *const kPdcartesTarotVersion;
extern const char *const kPdcartesTarotBuildVersion;

inline QString appVersion() { return QString::fromLatin1(kPdcartesTarotVersion); }
inline QString buildVersion() { return QString::fromLatin1(kPdcartesTarotBuildVersion); }

// Les deux ensemble, pour une ligne qui a la place des deux -- la version
// seule quand c'est une version, puisqu'il n'y a pas de build à en
// distinguer.
inline QString fullVersion() {
    const QString build = buildVersion();
    if (build.isEmpty()) {
        return appVersion();
    }
    return appVersion() + QStringLiteral(" (build ") + build + QLatin1Char(')');
}
