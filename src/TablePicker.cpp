#include "TablePicker.h"

#include "AskDialog.h"
#include "Cards.h"
#include "FeltArt.h"
#include "Fonts.h"
#include "MenuStyle.h"
#include "NameDialog.h"
#include "Palette.h"
#include "Profiles.h"
#include "RivalCard.h"
#include "Rivals.h"
#include "Tables.h"
#include "TileButton.h"
#include "TitleMark.h"

#include <QAction>
#include <QFontInfo>
#include <QEasingCurve>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

constexpr QSize kTableTile(188, 170);

// Ce qui sépare deux choses posées côte à côte sur le drap, tuiles et cartes
// confondues. Nommé plutôt qu'écrit deux fois, parce que c'est de lui que
// dépend l'accord des deux largeurs ci-dessous.
constexpr int kFeltGap = 10;

// La rangée des cinq tables fait la largeur du bloc, et la grille des six s'y
// range : trois colonnes qui se partagent exactement la place des cinq
// tuiles, pour que les deux sections aient le même bord droit. Calculé, et
// non écrit : une tuile qui grandit emmène les cartes avec elle.
constexpr int kBlockWidth = 5 * kTableTile.width() + 4 * kFeltGap;
constexpr int kRivalCardWidth = (kBlockWidth - 2 * kFeltGap) / 3;

// La marge de la page, la bande du logo, et la hauteur du logo dedans.
constexpr int kMargin = 34;
constexpr int kHeaderHeight = 114;
// La ligne du dessus ne fait que le cinquième de la grande : le logo en tête
// de page est d'autant plus haut, sans quoi `PDCartes` y tomberait sous la
// taille où ce trait de fonte se tient encore.
constexpr int kMarkHeight = 82;

// Ce qui sépare une section de la suivante, et ce qui sépare un titre de
// section de ce qu'il annonce. Le premier est large et le second serré :
// c'est l'écart qui dit à quoi un titre appartient, et deux sections
// également espacées de leur titre et l'une de l'autre n'en font qu'une
// longue.
constexpr int kSectionGap = 44;
constexpr int kHeadingGap = 10;
// Le temps que la page met à paraître derrière le logo qui se pose. Un peu
// plus court que le vol : le menu est là quand le logo arrive, et non après.
constexpr int kEntranceMs = 430;

// Le titre d'une section : deux mots d'or en capitales, et un filet qui
// court jusqu'au bout de la page. Une étiquette et une règle plutôt que la
// chrome des dialogues, qui est faite pour un fond pâle.
//
// En Zen Dots, la fonte de la ligne du dessus du logo : les titres de section
// sont ce que la page a de plus proche d'une enseigne, et les voir dans la
// fonte du logo les rattache à lui. Elle n'a pas de bas-de-casse -- ses
// minuscules sont ses capitales -- si bien que le texte source garde sa casse
// ordinaire et que `AllUppercase` ne sert qu'au repli : sans la fonte, le
// titre est en capitales quand même.
//
// De l'interlettrage, et un corps un peu réduit : Zen Dots est déjà large, et
// des capitales serrées font un bloc plutôt que des mots.
QWidget *feltSection(QWidget *parent, const QString &text) {
    auto *heading = new QWidget(parent);
    heading->setObjectName(QStringLiteral("feltSection"));
    auto *across = new QHBoxLayout(heading);
    across->setContentsMargins(0, 0, 0, 0);
    across->setSpacing(12);

    auto *label = new QLabel(text, heading);
    label->setObjectName(QStringLiteral("feltSectionLabel"));
    const qreal body = QFontInfo(label->font()).pixelSize();
    QFont caps = familyFont(label->font(), capsFamily(), body * 0.88);
    caps.setCapitalization(QFont::AllUppercase);
    caps.setLetterSpacing(QFont::PercentageSpacing, 118);
    label->setFont(caps);

    auto *rule = new QFrame(heading);
    rule->setObjectName(QStringLiteral("feltSectionRule"));
    rule->setFrameShape(QFrame::HLine);
    rule->setFixedHeight(1);

    across->addWidget(label);
    across->addWidget(rule, 1);
    return heading;
}

} // namespace

TablePicker::TablePicker(QWidget *parent) : QWidget(parent) {
    // Le tapis couvre tout, et c'est nous qui le peignons.
    setAttribute(Qt::WA_OpaquePaintEvent);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // La bande du logo ne défile pas : le logo est l'enseigne de la page, et
    // une enseigne qui s'en va au premier cran de molette n'en est pas une.
    // Elle ne peint rien elle-même -- c'est la page qui dessine le logo
    // dedans, pour que le vol depuis l'écran titre arrive sur le tapis et non
    // sur un widget.
    m_header = new QWidget(this);
    m_header->setFixedHeight(kHeaderHeight);
    m_header->setAttribute(Qt::WA_TransparentForMouseEvents);
    outer->addWidget(m_header);

    m_content = new QWidget(this);
    auto *down = new QVBoxLayout(m_content);
    down->setContentsMargins(0, 0, 0, 0);
    down->setSpacing(0);
    outer->addWidget(m_content, 1);

    auto *scroller = new QScrollArea(m_content);
    scroller->setWidgetResizable(true);
    scroller->setFrameShape(QFrame::NoFrame);
    // Une barre horizontale quand la fenêtre est trop étroite pour les cinq
    // tables : sans elle, la dernière est coupée et hors d'atteinte. Elle ne
    // paraît que là.
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Le drap se voit à travers : ni la zone, ni sa vitre, ni la page dedans
    // ne peignent de fond.
    scroller->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: 0; }"));
    scroller->viewport()->setAutoFillBackground(false);
    scroller->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    down->addWidget(scroller, 1);

    auto *page = new QWidget(scroller);
    page->setAutoFillBackground(false);
    page->setStyleSheet(QStringLiteral("background: transparent;"));
    scroller->setWidget(page);

    auto *rows = new QVBoxLayout(page);
    rows->setContentsMargins(kMargin, 0, kMargin, 22);
    rows->setSpacing(0);
    // Un ressort en tête : la page se tient au milieu de la place qu'elle a,
    // et se range en haut dès qu'elle n'en a plus -- un ressort rendu à zéro
    // est un ressort qui ne fait rien, et c'est alors la barre de défilement
    // qui prend la suite.
    rows->addStretch(1);

    // Les six. Avant les tables, bien qu'on puisse partir sans en inviter un
    // seul : c'est un choix, et un choix passé sous le déclencheur est un
    // choix qu'on fait après coup, la partie commencée. Au tarot il
    // compte double -- le premier invité est votre partenaire.
    QWidget *rivalHeading = feltSection(page, tr("Les joueurs"));
    m_headings.append(rivalHeading);
    rows->addWidget(rivalHeading);
    rows->addSpacing(kHeadingGap);
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(kFeltGap);
    grid->setVerticalSpacing(kFeltGap);
    // Une quatrième colonne qui ne porte rien et prend tout le reste : sans
    // elle les trois autres s'étirent jusqu'au bord de la fenêtre, et la
    // grille cesse de s'aligner sur les tuiles dès qu'on élargit.
    grid->setColumnStretch(3, 1);
    int at = 0;
    for (const Rival &rival : rivals()) {
        auto *card = new RivalCard(rival.id, page);
        card->setPickable(true);
        card->setOnFelt(true);
        // Fixe, comme une tuile : sur une fenêtre étroite les deux sections
        // débordent ensemble et la même barre horizontale les rattrape,
        // plutôt que l'une se resserre pendant que l'autre tient bon.
        card->setFixedWidth(kRivalCardWidth);
        card->setToolTip(tr("Cliquer pour inviter %1 à la table ; clic droit pour le "
                            "renommer ou le remettre à son niveau de départ.")
                             .arg(rivalName(rival.id)));
        const QString id = rival.id;
        connect(card, &RivalCard::invitedChanged, this, [this, card, id] {
            // L'ordre des invitations est gardé ici : c'est lui qui décide qui
            // s'assoit en face, et une case cochée ne le dit pas.
            m_order.removeAll(id);
            if (card->invited()) m_order.append(id);
            reread();
        });
        connect(card, &RivalCard::menuWanted, this,
                [this, card](const QPoint &where) { showRivalMenu(card, where); });
        grid->addWidget(card, at / 3, at % 3);
        m_cards.append(card);
        ++at;
    }
    rows->addLayout(grid);
    rows->addSpacing(12);

    // Ce que les invitations ont fait de la table : qui est en face, qui est
    // contre vous. Une ligne, sous la grille à laquelle elle répond.
    m_rivalNote = new QLabel(page);
    m_rivalNote->setWordWrap(true);
    m_rivalNote->setFixedWidth(kBlockWidth);
    rows->addWidget(m_rivalNote);
    rows->addSpacing(kSectionGap);

    // Les tables, en dernier : elles ne se choisissent pas, elles partent.
    QWidget *tableHeading = feltSection(page, tr("La table"));
    m_headings.append(tableHeading);
    rows->addWidget(tableHeading);
    rows->addSpacing(kHeadingGap);
    auto *tiles = new QHBoxLayout;
    tiles->setSpacing(kFeltGap);
    for (const TableKind &kind : tableKinds()) {
        auto *tile = new TileButton(page);
        tile->setText(kind.name);
        tile->setBlurb(kind.blurb);
        tile->setTileSize(kTableTile);
        tile->setOnFelt(true);
        tile->setCorner(countedText(kind.donnes, tr("donne"), tr("donnes")));
        const QString id = kind.id;
        connect(tile, &TileButton::clicked, this, [this, id] { Q_EMIT tableChosen(id); });
        m_tiles.append(tile);
        tiles->addWidget(tile);
    }
    tiles->addStretch();
    rows->addLayout(tiles);
    rows->addSpacing(12);

    // Les gestes qu'on ne devine pas, et rien d'autre : tout le reste est
    // écrit sur la chose elle-même. Au pied des tuiles plutôt qu'au pied de
    // la fenêtre, pour que la ligne tienne à ce qu'elle explique -- et ses
    // membres dans l'ordre de la page, celui de la table en dernier, qui est
    // celui de ce qu'elle a juste au-dessus.
    m_footer = new QLabel(page);
    m_footer->setWordWrap(true);
    m_footer->setText(tr("Un clic invite  ·  un clic droit renomme  ·  "
                         "une table lance la partie"));
    rows->addWidget(m_footer);
    rows->addStretch(1);

    reread();
}

QRectF TablePicker::markRect() const {
    const qreal height = kMarkHeight;
    const qreal width = height * titleMarkAspect(font());
    const qreal top = m_header ? m_header->y() : 0.0;
    return QRectF(kMargin, top + (kHeaderHeight - height) / 2.0 + 4.0, width, height);
}

void TablePicker::setMarkHidden(bool hidden) {
    if (m_markHidden == hidden) return;
    m_markHidden = hidden;
    update();
}

void TablePicker::beginEntrance() {
    if (!m_content) return;
    // L'effet n'est posé que le temps de la montée : il fait passer toute la
    // page par un pixmap, ce qui n'a pas à durer au-delà.
    auto *fade = new QGraphicsOpacityEffect(m_content);
    fade->setOpacity(0.0);
    m_content->setGraphicsEffect(fade);

    auto *rise = new QPropertyAnimation(fade, "opacity", this);
    rise->setDuration(kEntranceMs);
    rise->setStartValue(0.0);
    rise->setEndValue(1.0);
    rise->setEasingCurve(QEasingCurve::OutCubic);
    connect(rise, &QPropertyAnimation::finished, m_content,
            [content = m_content] { content->setGraphicsEffect(nullptr); });
    rise->start(QAbstractAnimation::DeleteWhenStopped);
}

void TablePicker::rebuildFelt() {
    if (width() <= 0 || height() <= 0) {
        m_felt = QPixmap();
        return;
    }
    const qreal ratio = devicePixelRatioF();
    m_felt = QPixmap(QSize(int(std::ceil(width() * ratio)), int(std::ceil(height() * ratio))));
    m_felt.setDevicePixelRatio(ratio);

    QPainter painter(&m_felt);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawFelt(&painter, QRectF(0, 0, width(), height()), currentPalette(), false);
}

void TablePicker::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    if (m_felt.isNull()) rebuildFelt();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawPixmap(0, 0, m_felt);
    if (!m_markHidden) drawTitleMark(&painter, markRect(), font(), currentPalette());
}

void TablePicker::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    rebuildFelt();
}

QVector<QString> TablePicker::invited() const { return m_order; }

void TablePicker::showRivalMenu(RivalCard *card, const QPoint &where) {
    const Rival &who = rivalById(card->rivalId());
    const Learning learning = learningFor(who.id);

    QMenu menu(this);
    dressMenu(&menu);
    const QString called = rivalName(who.id);
    QAction *invite = menu.addAction(card->invited() ? tr("Ne pas inviter %1").arg(called)
                                                     : tr("Inviter %1 à la table").arg(called));
    menu.addSeparator();
    QAction *rename = menu.addAction(tr("Renommer…"));
    menu.addSeparator();
    QAction *reset = menu.addAction(tr("Niveau de départ"));
    // Rien à remettre quand il n'a rien appris : l'entrée le dit en étant
    // éteinte, plutôt qu'en posant une question dont la réponse ne fait rien.
    reset->setEnabled(learning.xp > who.startXp);
    reset->setToolTip(
        tr("Remettre %1 au niveau avec lequel il arrive, sur ce profil seulement")
            .arg(called));

    const QAction *chosen = menu.exec(where);
    if (chosen == invite) {
        card->setInvited(!card->invited());
    } else if (chosen == rename) {
        renameRival(who.id);
    } else if (chosen == reset) {
        askResetRival(who.id);
    }
}

void TablePicker::renameRival(const QString &rivalId) {
    NameDialog dialog(tr("Renommer"), tr("Comment s'appelle ce joueur ?"),
                      tr("Son médaillon en prend l'initiale. Il garde sa couleur, son "
                         "expérience et sa façon de jouer. Laissez vide pour lui rendre "
                         "son nom."),
                      rivalName(rivalId), kRivalNameLength, this);
    if (dialog.exec() != QDialog::Accepted) return;
    // Un nom vide le fait répondre au sien : c'est setRivalName qui s'en
    // charge, et c'est la façon de le débaptiser.
    setRivalName(rivalId, dialog.name());
    reread();
}

void TablePicker::askResetRival(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    if (!askYesNo(this, tr("Remettre à zéro"),
                  tr("Remettre %1 à son niveau de départ ?").arg(rivalName(rivalId)),
                  tr("Remettre à zéro"),
                  tr("Sur « %1 » : %2 points d'expérience, soit %3.")
                      .arg(profileName(currentProfile()))
                      .arg(who.startXp)
                      .arg(levelName(levelOf(who.startXp)).toLower()))) {
        return;
    }
    resetRival(rivalId);
    reread();
}

void TablePicker::reread() {
    // Les cartes disent où chacun s'assoira, pour que le partenaire ne soit
    // pas une surprise de la première donne.
    QStringList names;
    for (int i = 0; i < m_order.size(); ++i) names.append(rivalName(m_order.at(i)));
    for (RivalCard *card : m_cards) {
        const int rank = m_order.indexOf(card->rivalId());
        card->setInvited(rank >= 0);
        if (rank < 0) {
            card->setSeatNote(QString());
        } else if (rank == 0) {
            card->setSeatNote(tr("en face de vous"));
        } else if (rank < kSeatCount - 1) {
            card->setSeatNote(tr("contre vous"));
        } else {
            card->setSeatNote(tr("pas de place"));
        }
        card->reread();
    }

    if (names.isEmpty()) {
        m_rivalNote->setText(tr("Les trois places vont aux moins joués, pour que les six "
                                "progressent ensemble. Cliquez sur une carte pour inviter "
                                "quelqu'un : le premier invité s'assoit en face de vous. "
                                "c'est donc lui qui joue avec vous."));
        return;
    }
    m_rivalNote->setText(tr("Votre partenaire : %1. %2Les places qui restent vont aux moins "
                            "joués.")
                             .arg(names.first(),
                                  names.size() > 1
                                      ? tr("Contre vous : %1. ")
                                            .arg(QStringList(names.mid(1, kSeatCount - 2))
                                                     .join(QStringLiteral(", ")))
                                      : QString()));
}

void TablePicker::refresh() {
    const Palette &scheme = currentPalette();
    rebuildFelt();

    for (QWidget *heading : m_headings) {
        if (auto *label = heading->findChild<QLabel *>(QStringLiteral("feltSectionLabel"))) {
            label->setStyleSheet(
                QStringLiteral("color: %1; background: transparent;").arg(scheme.gold.name()));
        }
        if (auto *rule = heading->findChild<QFrame *>(QStringLiteral("feltSectionRule"))) {
            rule->setStyleSheet(QStringLiteral("background: %1; border: 0;")
                                    .arg(feltGold(scheme, 0.28).name(QColor::HexArgb)));
        }
    }

    QColor aside = scheme.feltText;
    aside.setAlphaF(0.72);
    m_rivalNote->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg(aside.name(QColor::HexArgb)));
    aside.setAlphaF(0.58);
    m_footer->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                .arg(aside.name(QColor::HexArgb)));

    reread();
    update();
}
