#include "ScoreBoard.h"

#include "FeltArt.h"
#include "Palette.h"
#include "Tarot.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>

#include <algorithm>

namespace {

constexpr qreal kPad = 26.0;
constexpr qreal kRowHeight = 30.0;
constexpr qreal kNameWidth = 186.0;
constexpr qreal kNumberWidth = 74.0;
constexpr int kColumns = 4;

// Un nombre marqué, qui porte toujours son signe : une feuille de tarot
// s'additionne, et un gain qui ne se distingue pas d'une perte ne
// s'additionne pas.
QString signedText(int halves) {
    const QString written = pointsText(std::abs(halves));
    if (halves > 0) return QStringLiteral("+") + written;
    if (halves < 0) return QStringLiteral("−") + written;
    return QStringLiteral("0");
}

} // namespace

ScoreBoard::ScoreBoard(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NoSystemBackground);
    hide();

    m_onwards = new QPushButton(this);
    m_away = new QPushButton(this);
    m_onwards->setCursor(Qt::PointingHandCursor);
    m_away->setCursor(Qt::PointingHandCursor);
    connect(m_onwards, &QPushButton::clicked, this, &ScoreBoard::goOn);
    connect(m_away, &QPushButton::clicked, this, &ScoreBoard::leave);
}

void ScoreBoard::showDonne(const Tarot *game) {
    if (!game) return;
    m_matchOver = false;
    m_youWon = false;
    m_lines.clear();
    m_notes.clear();

    const DonneResult &done = game->result();
    int me = 0;
    for (int at = 0; at < game->seatCount(); ++at) {
        if (game->seat(at).human()) me = at;
    }

    auto present = [&](const QString &onwards) {
        m_onwards->setText(onwards);
        m_away->setText(tr("Quitter la table"));
        dressButtons();
        show();
        raise();
        setFocus();
        placeButtons();
        update();
    };

    if (done.annulled) {
        m_headline = tr("Donne annulée");
        m_aside = tr("Les %1 ont passé : personne n'avait de quoi prendre, et c'est au "
                     "suivant de donner.")
                      .arg(countedText(game->seatCount(), tr("joueur"), tr("joueurs")));
        present(tr("Redonner"));
        return;
    }

    // Votre ligne d'abord, puis les trois autres dans l'ordre où ils sont
    // assis : une feuille de marque se lit en commençant par sa propre ligne.
    int bestGain = done.gained[0];
    const int seats = game->seatCount();
    for (int at = 1; at < seats; ++at) bestGain = std::max(bestGain, done.gained[at]);
    for (int step = 0; step < seats; ++step) {
        const int at = (me + step) % seats;
        const int team = game->teamOf(at);
        Line line;
        line.name = game->seat(at).human() ? tr("Vous") : game->seat(at).name;
        line.ours = at == me;
        line.hue = line.ours ? currentPalette().hue : game->seat(at).hue;
        line.taker = at == done.taker;
        // Appel caché : la feuille ne dit « appelé » que si la révélation a
        // eu lieu (à la fin d'une donne, la carte appelée a toujours été
        // jouée, donc c'est le cas ; le garde-fou reste pour la propreté).
        line.callee = at == done.callee && (!game->callHidden() || game->callRevealed());
        line.tricks = done.tricks[team];
        line.points = done.points[team];
        line.gained = done.gained[at];
        line.total = game->seatScore(at);
        line.winner = done.gained[at] == bestGain && bestGain > 0;
        m_lines.append(line);
    }

    const bool ourContract = done.taker == me;
    if (done.chelemMade) {
        m_headline = ourContract ? tr("Chelem !") : tr("Chelem contre vous");
    } else if (done.made) {
        m_headline = ourContract ? tr("Contrat tenu") : tr("Le contrat passe");
    } else {
        m_headline = ourContract ? tr("Chute") : tr("Le contrat chute");
    }

    const QString name = game->seat(done.taker).human() ? tr("vous") : game->seat(done.taker).name;
    m_aside = tr("Donne %1 sur %2 — %3 par %4")
                  .arg(game->donneNumber())
                  .arg(game->targetDonnes())
                  .arg(contractName(done.contract), name);

    // Le compte du contrat, dit en entier : c'est la seule chose du tarot
    // qu'il faut avoir comprise, et elle tient en une ligne.
    m_notes.append(tr("%1 : il fallait %2 points, le preneur en a fait %3 — %4 %5")
                       .arg(countedText(done.bouts, tr("bout"), tr("bouts")),
                            pointsText(done.target), pointsText(done.points[kTakerTeam]),
                            done.made ? tr("soit") : tr("soit un manque de"),
                            pointsText(std::abs(done.ecart))));

    if (done.petitAuBoutTeam >= 0) {
        const bool ours = (done.petitAuBoutTeam == kTakerTeam) == ourContract;
        m_notes.append(ours ? tr("petit au bout pour votre camp : 10 points, multipliés par le "
                                 "contrat")
                            : tr("petit au bout pour l'autre camp : 10 points, multipliés par "
                                 "le contrat"));
    }
    if (done.poigneePoints > 0) {
        const QString who = done.poigneeSeat == me ? tr("vous")
                                                   : game->seat(done.poigneeSeat).name;
        m_notes.append(tr("%1 a montré une %2 : %3 points, acquis au camp qui gagne la donne")
                           .arg(who, poigneeName(done.poignee), pointsText(done.poigneePoints)));
    }
    if (done.chelemAnnounced) {
        m_notes.append(done.chelemMade ? tr("chelem annoncé et réussi : 400 points")
                                       : tr("chelem annoncé et manqué : 200 points perdus"));
    } else if (done.chelem != 0) {
        m_notes.append(tr("chelem sans annonce : 200 points"));
    }
    m_notes.append(tr("chaque défenseur %1 %2, le preneur trois fois cela")
                       .arg(done.unit >= 0 ? tr("paie") : tr("reçoit"),
                            pointsText(std::abs(done.unit))));

    present(tr("Donne suivante"));
}

void ScoreBoard::showMatch(const Tarot *game) {
    if (!game) return;
    showDonne(game);
    m_matchOver = true;

    int me = 0;
    for (int at = 0; at < game->seatCount(); ++at) {
        if (game->seat(at).human()) me = at;
    }
    const int winner = game->matchWinner();
    m_youWon = winner == me;

    int best = game->seatScore(0);
    for (int at = 1; at < game->seatCount(); ++at) best = std::max(best, game->seatScore(at));
    for (Line &line : m_lines) line.winner = line.total == best;

    if (winner < 0) {
        m_headline = tr("Partie nulle");
    } else {
        m_headline = m_youWon ? tr("Partie gagnée") : tr("Partie perdue");
    }
    m_aside = tr("%1 — vous finissez à %2")
                  .arg(countedText(game->donneNumber(), tr("donne jouée"),
                                   tr("donnes jouées")),
                       signedText(game->seatScore(me)));

    m_onwards->setText(tr("Rejouer"));
    m_away->setText(tr("Quitter la table"));
    dressButtons();
    placeButtons();
    update();
}

void ScoreBoard::hideBoard() { hide(); }

void ScoreBoard::refresh() {
    dressButtons();
    update();
}

void ScoreBoard::dressButtons() {
    const Palette &scheme = currentPalette();
    const QString common =
        QStringLiteral("QPushButton { border-radius: 7px; padding: 7px 20px 9px 20px;"
                       " font-weight: bold; border: 1px solid %1; }");
    m_onwards->setStyleSheet(
        common.arg(scheme.accent.name()) +
        QStringLiteral("QPushButton { background: %1; color: %2; }"
                       "QPushButton:hover { background: %3; }")
            .arg(scheme.accent.name(), scheme.accentText.name(),
                 blendColours(scheme.accent, QColor(Qt::white), 0.16).name()));
    m_away->setStyleSheet(common.arg(scheme.edge.name()) +
                          QStringLiteral("QPushButton { background: %1; color: %2; }"
                                         "QPushButton:hover { border: 1px solid %3; }")
                              .arg(scheme.surface.name(), scheme.surfaceText.name(),
                                   scheme.accent.name()));
}

QSizeF ScoreBoard::measure() const {
    const qreal rows = kRowHeight * m_lines.size();
    const qreal width = kPad * 2 + kNameWidth + kNumberWidth * kColumns;
    const qreal notes = m_notes.isEmpty() ? 0.0 : 18.0 * m_notes.size() + 8.0;
    const qreal height = kPad * 2 + 46.0 + 22.0 + 20.0 + rows + notes + 30.0 + 40.0;
    return QSizeF(width, height);
}

void ScoreBoard::placeButtons() {
    const QSizeF card = measure();
    QRectF box(0, 0, card.width(), card.height());
    box.moveCenter(QPointF(width() / 2.0, height() / 2.0));

    const qreal buttonTop = box.bottom() - kPad - 34.0;
    m_onwards->adjustSize();
    m_away->adjustSize();
    m_onwards->move(int(box.right() - kPad - m_onwards->width()), int(buttonTop));
    m_away->move(int(box.left() + kPad), int(buttonTop));
}

void ScoreBoard::resizeEvent(QResizeEvent *) { placeButtons(); }

void ScoreBoard::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        Q_EMIT goOn();
        return;
    case Qt::Key_Escape:
        Q_EMIT leave();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void ScoreBoard::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QColor veil = scheme.feltEdge;
    veil.setAlpha(206);
    painter.fillRect(rect(), veil);

    const QSizeF size = measure();
    QRectF box(0, 0, size.width(), size.height());
    box.moveCenter(QPointF(width() / 2.0, height() / 2.0));

    QPainterPath shape;
    shape.addRoundedRect(box, 16, 16);
    painter.setPen(Qt::NoPen);
    painter.fillPath(shape, scheme.surface);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(m_matchOver && m_youWon ? scheme.gold : scheme.accent, 2.2));
    painter.drawPath(shape);

    const QRectF inner = box.adjusted(kPad, kPad, -kPad, -kPad);

    QFont headline = plainFont(&painter);
    headline.setBold(true);
    headline.setPixelSize(m_matchOver ? 26 : 21);
    painter.setFont(headline);
    painter.setPen(m_matchOver && m_youWon ? scheme.gold : scheme.accent);
    const QFontMetricsF headMetrics(headline);
    painter.drawText(QRectF(inner.left(), inner.top(), inner.width(), headMetrics.height()),
                     Qt::AlignCenter, m_headline);
    qreal y = inner.top() + headMetrics.height() + 3.0;

    QFont aside = plainFont(&painter);
    aside.setPixelSize(12);
    painter.setFont(aside);
    painter.setPen(scheme.hint);
    const QFontMetricsF asideMetrics(aside);
    painter.drawText(QRectF(inner.left(), y, inner.width(), asideMetrics.height() * 2.0),
                     Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_aside);
    y += asideMetrics.height() + 16.0;

    if (!m_lines.isEmpty()) {
        // L'en-tête des colonnes. Quatre chiffres par joueur, et ils ne disent
        // pas la même chose : les plis de son camp, les points que son camp a
        // ramassés, ce que lui marque, et son total depuis le début.
        QFont heading = plainFont(&painter);
        heading.setBold(true);
        heading.setPixelSize(10);
        heading.setCapitalization(QFont::AllUppercase);
        heading.setLetterSpacing(QFont::PercentageSpacing, 120);
        painter.setFont(heading);
        painter.setPen(scheme.hint);
        const QStringList columns = {tr("plis"), tr("ramassés"), tr("marqués"), tr("total")};
        for (int i = 0; i < columns.size(); ++i) {
            painter.drawText(
                QRectF(inner.left() + kNameWidth + kNumberWidth * i, y, kNumberWidth, 14.0),
                Qt::AlignRight | Qt::AlignVCenter, columns.at(i));
        }
        y += 16.0;
        painter.setPen(QPen(scheme.edge, 1.0));
        painter.drawLine(QPointF(inner.left(), y), QPointF(inner.right(), y));
        y += 3.0;

        QFont row = plainFont(&painter);
        row.setPixelSize(14);
        QFont rowBold = row;
        rowBold.setBold(true);

        for (const Line &line : m_lines) {
            const QRectF band(inner.left(), y, inner.width(), kRowHeight);
            if (line.winner) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(blendColours(scheme.surface, scheme.accent, 0.14));
                painter.drawRoundedRect(band.adjusted(-6, 1, 6, -1), 6, 6);
            }

            painter.setPen(Qt::NoPen);
            painter.setBrush(rivalColour(line.hue));
            painter.drawEllipse(QRectF(band.left(), band.center().y() - 4.5, 9.0, 9.0));

            painter.setFont(line.ours ? rowBold : row);
            painter.setPen(scheme.surfaceText);
            // Le preneur est marqué sur la feuille : sans cela, quatre lignes
            // de chiffres ne disent pas qui était seul contre les trois
            // autres.
            const QString name =
                line.taker  ? tr("%1 — preneur").arg(line.name)
                : line.callee ? tr("%1 — appelé").arg(line.name)
                              : line.name;
            painter.drawText(QRectF(band.left() + 16.0, band.top(), kNameWidth - 16.0, kRowHeight),
                             Qt::AlignLeft | Qt::AlignVCenter, name);

            const QString figures[kColumns] = {QString::number(line.tricks),
                                               pointsText(line.points), signedText(line.gained),
                                               signedText(line.total)};
            for (int i = 0; i < kColumns; ++i) {
                const bool last = i == kColumns - 1;
                const bool marked = i == kColumns - 2;
                painter.setFont(last || marked ? rowBold : row);
                QColor ink = scheme.surfaceText;
                if (marked && line.gained < 0) ink = scheme.danger;
                if (last && line.winner) ink = scheme.accent;
                painter.setPen(ink);
                painter.drawText(QRectF(band.left() + kNameWidth + kNumberWidth * i, band.top(),
                                        kNumberWidth, kRowHeight),
                                 Qt::AlignRight | Qt::AlignVCenter, figures[i]);
            }
            y += kRowHeight;
        }
        y += 8.0;
    }

    if (m_notes.isEmpty()) return;
    QFont note = plainFont(&painter);
    note.setPixelSize(11);
    painter.setFont(note);
    painter.setPen(scheme.hint);
    for (const QString &said : m_notes) {
        painter.drawText(QRectF(inner.left(), y, inner.width(), 18.0),
                         Qt::AlignLeft | Qt::AlignVCenter, said);
        y += 18.0;
    }
}
