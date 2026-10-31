#include "Tables.h"

#include <QCoreApplication>

namespace {

const QVector<TableKind> &builtTables() {
    static const QVector<TableKind> kinds = {
        {QStringLiteral("donne"), QCoreApplication::translate("tables", "Une donne"),
         QCoreApplication::translate("tables", "Une seule, et la meilleure marque l'emporte"), 1},
        {QStringLiteral("tour"), QCoreApplication::translate("tables", "Le tour de table"),
         QCoreApplication::translate("tables", "Quatre donnes : chacun donne une fois"), 4},
        {QStringLiteral("tapis"), QCoreApplication::translate("tables", "Le tapis vert"),
         QCoreApplication::translate("tables", "Huit donnes, le tarot tel qu'on le joue"), 8},
        {QStringLiteral("cercle"), QCoreApplication::translate("tables", "Le cercle"),
         QCoreApplication::translate("tables", "Douze donnes : de quoi se refaire"), 12},
        {QStringLiteral("soiree"), QCoreApplication::translate("tables", "La soirée"),
         QCoreApplication::translate("tables", "Vingt donnes : les six y apprennent"), 20},
    };
    return kinds;
}

} // namespace

const QVector<TableKind> &tableKinds() { return builtTables(); }

const TableKind &tableKindById(const QString &id) {
    for (const TableKind &kind : builtTables()) {
        if (kind.id == id) return kind;
    }
    // La table par défaut, pour qu'un fichier de réglages écrit par une
    // version plus tardive ne laisse pas le jeu sans table du tout.
    return builtTables().at(2);
}

QString defaultTableId() { return builtTables().at(2).id; }
