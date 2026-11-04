#include "SkillRow.h"

#include "FeltArt.h"
#include "Palette.h"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>

namespace {

// Position des colonnes, mesurée depuis la gauche. Ces valeurs sont fixes et
// non calculées : huit lignes alignées se lisent comme un tableau, alors que
// huit lignes alignées chacune sur son propre texte se lisent isolément.
constexpr qreal kTitleWide = 112.0;
constexpr qreal kPipStep = 18.0;
constexpr qreal kPipSize = 11.0;
constexpr qreal kRomanWide = 34.0;

// Mode compact : largeur des mêmes colonnes dans l'aperçu. Les deux colonnes
// de compétences y figurent côte à côte, et huit lignes à l'interligne de la
// fiche produiraient un panneau plus haut que la plaque survolée.
constexpr qreal kTightTitle = 76.0;
constexpr qreal kTightRoman = 24.0;
constexpr qreal kTightWord = 172.0;
constexpr qreal kTightHigh = 21.0;
// Corps du texte en mode compact.
constexpr int kTightType = 11;

} // namespace

SkillRow::SkillRow(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(int(kTightHigh));
}

qreal SkillRow::titleWide() const { return m_compact ? kTightTitle : kTitleWide; }

// La colonne du rang n'a de place qu'en mode compact si la ligne en a un :
// les deux colonnes de l'aperçu sont côte à côte, et celle des innées,
// dépourvue de rang, n'a pas à en réserver la place.
qreal SkillRow::romanWide() const {
    if (!m_compact) return kRomanWide;
    return m_roman.isEmpty() ? 0.0 : kTightRoman;
}

qreal SkillRow::wordWide() const { return m_compact ? kTightWord : 160.0; }

QSize SkillRow::sizeHint() const {
    // En mode compact, la ligne se limite à ses colonnes, la dernière étant
    // ajustée à son libellé : c'est ce qui détermine la largeur de l'aperçu,
    // quatre libellés courts n'ayant pas à réserver la place de quatre longs.
    if (m_compact) {
        QFont word = font();
        word.setBold(true);
        word.setPixelSize(kTightType);
        const qreal mine = QFontMetricsF(word).horizontalAdvance(m_word) + 4.0;
        return QSize(int(titleWide() + m_outOf * kPipStep + 8.0 + romanWide() +
                         std::clamp(mine, 40.0, kTightWord)),
                     int(kTightHigh));
    }
    return QSize(560, int(kTightHigh * 1.6));
}

void SkillRow::setTitle(const QString &title) {
    m_title = title;
    update();
}

void SkillRow::setHue(int hue) {
    m_hue = hue;
    update();
}

void SkillRow::setLevel(int level, int outOf) {
    m_outOf = std::max(1, outOf);
    m_level = qBound(0, level, m_outOf - 1);
    update();
}

void SkillRow::setWord(const QString &word) {
    m_word = word;
    // En mode compact, la largeur de la ligne est déterminée par son libellé.
    if (m_compact) updateGeometry();
    update();
}

void SkillRow::setRank(const QString &roman) {
    m_roman = roman;
    if (m_compact) updateGeometry();
    update();
}

void SkillRow::setCompact(bool compact) {
    if (m_compact == compact) return;
    m_compact = compact;
    setMinimumHeight(int(kTightHigh));
    updateGeometry();
    update();
}

QRectF SkillRow::pipBox(int index) const {
    const qreal top = (height() - kPipSize) / 2.0;
    return QRectF(titleWide() + index * kPipStep, top, kPipSize, kPipSize);
}

void SkillRow::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    const QColor mine = rivalColour(m_hue);
    const QColor empty = blendColours(scheme.surface, scheme.surfaceText, 0.18);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QFont name = plainFont(&painter);
    name.setBold(true);
    name.setPixelSize(m_compact ? kTightType + 1 : 14);
    painter.setFont(name);
    painter.setPen(scheme.surfaceText);
    painter.drawText(QRectF(0.0, 0.0, titleWide() - 8.0, height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_title);

    // Les pastilles : pleines jusqu'au niveau atteint, évidées au-delà.
    for (int i = 0; i < m_outOf; ++i) {
        const QRectF box = pipBox(i);
        painter.setPen(Qt::NoPen);
        painter.setBrush(i <= m_level ? mine : empty);
        painter.drawEllipse(box);
    }

    qreal left = titleWide() + m_outOf * kPipStep + 8.0;

    // Le rang, lorsqu'il existe (acquises seules).
    if (!m_roman.isEmpty()) {
        QFont rank = plainFont(&painter);
        rank.setBold(true);
        rank.setPixelSize(m_compact ? kTightType : 13);
        painter.setFont(rank);
        painter.setPen(mine);
        painter.drawText(QRectF(left, 0.0, romanWide(), height()),
                         Qt::AlignLeft | Qt::AlignVCenter, m_roman);
    }
    left += romanWide();

    QFont word = plainFont(&painter);
    word.setBold(true);
    word.setPixelSize(m_compact ? kTightType : 13);
    painter.setFont(word);
    painter.setPen(mine);
    const QFontMetricsF wordMetrics(word);
    // En mode compact, le libellé occupe tout l'espace restant : la largeur de
    // la colonne est celle de la colonne de compétences, déterminée par le plus
    // long de ses quatre libellés.
    const qreal wordRoom = m_compact ? width() - left : wordWide() - 8.0;
    painter.drawText(QRectF(left, 0.0, wordRoom, height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     wordMetrics.elidedText(m_word, Qt::ElideRight, wordRoom));
}
