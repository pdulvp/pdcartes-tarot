#include "RivalCard.h"

#include "FeltArt.h"
#include "Fonts.h"
#include "Palette.h"
#include "RivalGlance.h"
#include "Rivals.h"

#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>

namespace {

constexpr qreal kPad = 12.0;
constexpr qreal kCorner = 12.0;

} // namespace

RivalCard::RivalCard(const QString &rivalId, QWidget *parent)
    : QWidget(parent), m_rivalId(rivalId) {
    setAttribute(Qt::WA_Hover);
    setMinimumSize(286, 112);

    // L'aperçu n'apparaît qu'après un maintien du pointeur sur la carte : un
    // déplacement à travers les six n'en déclenche aucun.
    m_glanceBeat = new QTimer(this);
    m_glanceBeat->setSingleShot(true);
    m_glanceBeat->setInterval(kGlanceDelayMs);
    connect(m_glanceBeat, &QTimer::timeout, this, [this] {
        showRivalGlance(m_rivalId, QRect(mapToGlobal(QPoint(0, 0)), size()), this);
    });
}

QSize RivalCard::sizeHint() const { return QSize(300, 118); }

void RivalCard::setInvited(bool invited) {
    if (m_invited == invited) return;
    m_invited = invited;
    update();
    Q_EMIT invitedChanged();
}

void RivalCard::setSeatNote(const QString &note) {
    if (m_seatNote == note) return;
    m_seatNote = note;
    update();
}

void RivalCard::setPickable(bool pickable) {
    m_pickable = pickable;
    setCursor(pickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

void RivalCard::setOnFelt(bool onFelt) {
    m_onFelt = onFelt;
    update();
}

void RivalCard::reread() { update(); }

void RivalCard::mousePressEvent(QMouseEvent *event) {
    // Après un clic, l'aperçu n'a plus lieu d'être affiché, que ce soit pour
    // inviter le joueur ou pour demander le petit menu.
    m_glanceBeat->stop();
    hideRivalGlance(this);
    if (!m_pickable || event->button() != Qt::LeftButton) return;
    setInvited(!m_invited);
}

void RivalCard::contextMenuEvent(QContextMenuEvent *event) {
    m_glanceBeat->stop();
    hideRivalGlance(this);
    // La position à l'écran plutôt que dans la carte : la page qui dresse le
    // menu n'a pas à savoir où la carte se trouve.
    Q_EMIT menuWanted(event->globalPos());
    event->accept();
}

void RivalCard::enterEvent(QEnterEvent *) {
    m_hovered = true;
    m_glanceBeat->start();
    update();
}

void RivalCard::leaveEvent(QEvent *) {
    m_hovered = false;
    m_glanceBeat->stop();
    // L'aperçu est libéré et non fermé : le pointeur peut simplement être
    // passé sur celui-ci, l'aperçu ayant pu être placé sur la carte faute de
    // place ailleurs.
    releaseRivalGlance(this);
    update();
}

void RivalCard::hideEvent(QHideEvent *event) {
    // Changement de page ou fermeture de la fenêtre : l'aperçu doit être
    // fermé, faute de quoi il subsisterait sans la carte à laquelle il se
    // rapporte.
    m_glanceBeat->stop();
    hideRivalGlance(this);
    QWidget::hideEvent(event);
}

void RivalCard::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    const Rival &who = rivalById(m_rivalId);
    // Le nom qu'il porte sur ce profil. Le médaillon en prend l'initiale, si
    // bien que le rebaptiser redessine sa pastille.
    const QString called = rivalName(m_rivalId);
    const Learning learning = learningFor(who.id);
    const Level level = levelOf(learning.xp);
    const QColor mine = rivalColour(who.hue);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRectF box = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    QPainterPath shape;
    shape.addRoundedRect(box, kCorner, kCorner);

    const bool lit = m_hovered && m_pickable;
    QColor body;
    QColor edge;
    if (m_onFelt) {
        // Sur le drap : presque noir, un peu éclairci au survol, et un filet
        // d'or quand il est invité -- l'or est ce qui est retenu partout
        // ailleurs dans ce menu. Sa couleur à lui reste dans le médaillon,
        // dans le cerne de sa pastille et dans le corps de la plaque.
        body = blendColours(scheme.feltEdge, QColor(0, 0, 0), lit ? 0.42 : 0.52);
        if (m_invited) body = blendColours(body, mine, 0.18);
        body.setAlpha(lit || m_invited ? 215 : 205);
        edge = scheme.gold;
        edge.setAlphaF(m_invited ? 0.95 : (lit ? 0.70 : 0.22));
    } else {
        body = scheme.surface;
        if (m_invited) body = blendColours(body, mine, 0.16);
        else if (lit) body = blendColours(body, mine, 0.08);
        edge = m_invited ? mine : (lit ? blendColours(mine, scheme.edge, 0.4) : scheme.edge);
    }
    painter.setPen(Qt::NoPen);
    painter.fillPath(shape, body);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(edge, m_invited ? 2.2 : 1.0));
    painter.drawPath(shape);

    // L'encre de la carte, selon le fond sous elle.
    const QColor ink = m_onFelt ? scheme.feltText : scheme.surfaceText;
    QColor asideInk = m_onFelt ? scheme.feltText : scheme.hint;
    if (m_onFelt) asideInk.setAlphaF(0.62);
    const QColor behind =
        m_onFelt ? blendColours(scheme.feltEdge, QColor(0, 0, 0), 0.52) : scheme.surface;

    const QRectF inner = box.adjusted(kPad, kPad, -kPad, -kPad);
    const qreal disc = std::min(inner.height() * 0.72, 56.0);
    drawRivalEmblem(&painter, QRectF(inner.left(), inner.top(), disc, disc), who.emblem,
                    called, mine, blendColours(behind, mine, 0.14));

    const qreal left = inner.left() + disc + 12.0;
    const qreal wide = inner.right() - left;

    QFont name = plainFont(&painter);
    name.setBold(true);
    name.setPixelSize(15);
    painter.setFont(name);
    painter.setPen(ink);
    const QFontMetricsF nameMetrics(name);
    painter.drawText(QRectF(left, inner.top(), wide, nameMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, called);

    // Le niveau, en pastille, dans sa couleur.
    QFont pill = plainFont(&painter);
    pill.setPixelSize(10);
    pill.setBold(true);
    pill.setCapitalization(QFont::AllUppercase);
    pill.setLetterSpacing(QFont::PercentageSpacing, 115);
    painter.setFont(pill);
    const QFontMetricsF pillMetrics(pill);
    const QString badge = levelName(level);
    QRectF chip(0, 0, pillMetrics.horizontalAdvance(badge) + 16.0, pillMetrics.height() + 4.0);
    chip.moveTopRight(QPointF(inner.right(), inner.top() + 1.0));
    if (m_onFelt) {
        // Creuse plutôt que pleine : six pastilles pleines dans six couleurs
        // vives, sur un menu qui tient à l'or et au noir, c'est six taches.
        // Le cerne garde sa couleur, ce qui suffit à le reconnaître.
        painter.setPen(QPen(mine, 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(chip, chip.height() / 2.0, chip.height() / 2.0);
        painter.setPen(mine);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(mine);
        painter.drawRoundedRect(chip, chip.height() / 2.0, chip.height() / 2.0);
        painter.setPen(readableTextOn(mine));
    }
    painter.drawText(capCentred(chip, pillMetrics), Qt::AlignCenter, badge);

    qreal y = inner.top() + nameMetrics.height() + 1.0;

    QFont aside = plainFont(&painter);
    aside.setBold(false);
    aside.setPixelSize(11);
    aside.setCapitalization(QFont::MixedCase);
    aside.setLetterSpacing(QFont::PercentageSpacing, 100);
    painter.setFont(aside);
    painter.setPen(asideInk);
    const QFontMetricsF asideMetrics(aside);
    painter.drawText(QRectF(left, y, wide, asideMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, who.blurb);
    y += asideMetrics.height() + 5.0;

    // Les étoiles, la jauge, et le chiffre derrière elles -- parce que la
    // jauge dit "bientôt" et le chiffre dit de combien.
    drawLevelStars(&painter, QRectF(left, y, 46.0, 12.0), levelStars(level), 3, scheme.gold,
                   blendColours(behind, ink, 0.16));
    const qreal gaugeLeft = left + 54.0;
    const qreal gaugeWide = std::max(40.0, wide - 54.0 - 64.0);
    drawGauge(&painter, QRectF(gaugeLeft, y + 4.0, gaugeWide, 5.0), progressWithin(learning.xp),
              m_onFelt ? scheme.gold : mine, blendColours(behind, ink, 0.14));
    painter.setFont(aside);
    painter.setPen(asideInk);
    painter.drawText(QRectF(gaugeLeft + gaugeWide + 8.0, y - 1.0, 60.0, 14.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("%1 xp").arg(learning.xp));
    y += 16.0;

    // Ce qu'il a fait sous ce profil. Un joueur qui n'a jamais joué le dit,
    // parce que c'est la réponse à « pourquoi est-il encore débutant ».
    QString record;
    if (learning.donnesPlayed == 0) {
        record = tr("pas encore joué sous ce profil");
    } else {
        // Trois accords à faire à la main. Le pluriel de Qt demande un
        // fichier de traduction, et il n'y en a pas : le jeu est en français
        // et en français seulement.
        const QString donnes =
            learning.donnesPlayed > 1 ? tr("%1 donnes") : tr("%1 donne");
        const QString won = learning.donnesWon > 1 ? tr("%1 gagnées") : tr("%1 gagnée");
        const QString made = learning.contractsMade > 1 ? tr("%1 contrats tenus")
                                                        : tr("%1 contrat tenu");
        record = tr("%1, %2, %3")
                     .arg(donnes.arg(learning.donnesPlayed), won.arg(learning.donnesWon),
                          made.arg(learning.contractsMade));
    }
    // Sur toute la largeur, et coupé proprement s'il ne tient quand même pas :
    // une phrase coupée net au milieu d'un chiffre se lit comme un autre
    // chiffre.
    painter.setPen(asideInk);
    painter.drawText(QRectF(left, y, wide, asideMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     asideMetrics.elidedText(record, Qt::ElideRight, wide));

    if (m_pickable && m_invited) {
        QFont mark = plainFont(&painter);
        mark.setBold(true);
        mark.setPixelSize(11);
        painter.setFont(mark);
        painter.setPen(m_onFelt ? scheme.gold : mine);
        painter.drawText(inner, Qt::AlignRight | Qt::AlignBottom,
                         m_seatNote.isEmpty() ? tr("invité") : m_seatNote);
    }
}
