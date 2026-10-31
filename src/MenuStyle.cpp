#include "MenuStyle.h"

#include "Palette.h"

#include <QFile>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QString>

void dressMenu(QMenu *menu) {
    if (!menu) return;
    const Palette &scheme = currentPalette();
    menu->setStyleSheet(
        QStringLiteral("QMenu { background: %1; color: %2; border: 1px solid %3;"
                       " border-radius: 8px; padding: 5px; }"
                       // Un pixel de moins en haut qu'en bas, comme partout où Qt
                       // centre lui-même : voir `capCentred` dans Fonts.h.
                       "QMenu::item { padding: 5px 22px 7px 14px;"
                       " border-radius: 5px; }"
                       "QMenu::item:selected { background: %4; color: %5; }"
                       "QMenu::item:disabled { color: %6; }"
                       "QMenu::separator { height: 1px; background: %3; margin: 5px 8px; }")
            .arg(scheme.surface.name(), scheme.surfaceText.name(), scheme.edge.name(),
                 scheme.accent.name(), scheme.accentText.name(), scheme.hint.name()));
}

namespace {

// La même teinture que `tintedIcon`, mais sur une image qu'on apporte : la
// forme est gardée et la couleur jetée, ce qui est tout ce que teinter une
// icône d'un seul ton veut dire.
QIcon tintedFile(const QString &path, const QColor &ink) {
    const QIcon source(path);
    if (source.isNull()) return {};

    QIcon tinted;
    for (int size : {16, 22, 24, 32, 48}) {
        QPixmap art = source.pixmap(size, size);
        if (art.isNull()) continue;

        QPixmap coloured(art.size());
        coloured.setDevicePixelRatio(art.devicePixelRatio());
        coloured.fill(Qt::transparent);

        QPainter painter(&coloured);
        painter.drawPixmap(0, 0, art);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(coloured.rect(), ink);
        painter.end();

        tinted.addPixmap(coloured);
    }
    return tinted;
}

} // namespace

QIcon appIcon(const QStringList &names, const QColor &ink) {
    // Le bureau d'abord : sur une machine qui a un thème, le jeu porte les
    // icônes de la maison plutôt que les siennes.
    const QIcon theirs = tintedIcon(names, ink);
    if (!theirs.isNull()) return theirs;

    for (const QString &name : names) {
        const QString path = QStringLiteral(":/icons/%1.svg").arg(name);
        if (!QFile::exists(path)) continue;
        const QIcon ours = tintedFile(path, ink);
        if (!ours.isNull()) return ours;
    }
    return {};
}
