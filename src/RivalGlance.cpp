#include "RivalGlance.h"

#include "FeltArt.h"
#include "Rivals.h"

#include "CardArt.h"
#include "DialogChrome.h"
#include "Palette.h"

#include <QCursor>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

// Marge intérieure de l'aperçu, espacement entre ses deux colonnes, et
// interligne. Ces valeurs sont serrées : l'aperçu est un panneau posé à côté
// d'une plaque, et un panneau plus haut que la plaque qu'il décrit se lit mal.
constexpr int kPad = 14;
constexpr int kBetween = 24;
constexpr int kTight = 1;
constexpr qreal kCorner = 10.0;

// Largeur laissée à la petite phrase de l'adversaire : au-delà, elle passe à
// la ligne. Une phrase sur une seule ligne donnerait un panneau large comme la
// table.
constexpr int kBlurbWide = 196;

// Décalage entre l'aperçu et l'élément survolé, afin que leurs bords ne se
// touchent pas.
constexpr int kOffset = 6;

// Période de vérification de la position du pointeur, une fois l'aperçu
// affiché.
constexpr int kWatchMs = 120;

// Aperçu actuellement affiché et élément l'ayant demandé. Un seul aperçu est
// admis à la fois, deux aperçus simultanés décrivant deux adversaires
// différents.
QPointer<RivalGlance> g_glance;
QPointer<QWidget> g_about;

// Les étoiles du niveau, la jauge de progression à l'intérieur de celui-ci, et
// l'expérience en chiffres -- la jauge dit « bientôt », le chiffre dit de
// combien. C'est la ligne que porte déjà la carte du menu d'ouverture, reprise
// ici parce qu'elle est ce qu'on vient lire.
class GlanceLevelLine : public QWidget {
public:
    GlanceLevelLine(int xp, const QColor &mine, QWidget *parent)
        : QWidget(parent), m_xp(xp), m_mine(mine) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFixedHeight(14);
        setMinimumWidth(170);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const Palette &scheme = currentPalette();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QFont aside = plainFont(&painter);
        aside.setPixelSize(11);
        painter.setFont(aside);
        const QFontMetricsF metrics(aside);
        const QString count = RivalGlance::tr("%1 xp").arg(m_xp);
        const qreal countWide = metrics.horizontalAdvance(count) + 2.0;

        const QColor ink = scheme.surfaceText;
        drawLevelStars(&painter, QRectF(0.0, 1.0, 46.0, 12.0), levelStars(levelOf(m_xp)), 3,
                       scheme.gold, blendColours(scheme.surface, ink, 0.16));

        const qreal gaugeLeft = 54.0;
        const qreal gaugeWide = std::max(32.0, width() - gaugeLeft - countWide - 8.0);
        drawGauge(&painter, QRectF(gaugeLeft, 5.0, gaugeWide, 5.0), progressWithin(m_xp), m_mine,
                  blendColours(scheme.surface, ink, 0.14));

        painter.setPen(scheme.hint);
        painter.drawText(QRectF(gaugeLeft + gaugeWide + 8.0, 0.0, countWide + 4.0, 14.0),
                         Qt::AlignLeft | Qt::AlignVCenter, count);
    }

private:
    int m_xp = 0;
    QColor m_mine;
};

// Une ligne de texte de l'aperçu. La couleur est posée sur l'étiquette
// elle-même : l'aperçu est une fenêtre à part, qui n'hérite donc pas de la
// feuille de style d'une boîte de dialogue, et `markHintLabel` n'y produirait
// aucun effet.
QLabel *glanceLine(QWidget *parent, const QString &text, bool dim, int wide = 0) {
    auto *label = new QLabel(text, parent);
    QFont small = label->font();
    small.setPixelSize(11);
    label->setFont(small);
    QPalette ink = label->palette();
    ink.setColor(QPalette::WindowText, dim ? currentPalette().hint : currentPalette().surfaceText);
    label->setPalette(ink);
    if (wide > 0) {
        label->setWordWrap(true);
        label->setFixedWidth(wide);
    }
    return label;
}

// Détermine la position de l'aperçu : sous l'élément survolé, au-dessus, ou
// à côté -- la première des quatre positions tenant à l'écran sans recouvrir ni
// l'élément survolé ni le pointeur. Un panneau placé sous le pointeur capte le
// survol de l'élément qu'il décrit, ce qui provoquait un clignotement.
QRect glancePlace(const QSize &size, const QRect &avoid, const QRect &room, const QPoint &mouse) {
    const int w = size.width();
    const int h = size.height();
    // Aligné sur le bord qu'il longe, puis ramené dans les limites de
    // l'écran : un aperçu commençant au bord gauche de la carte se lit comme
    // son prolongement.
    const int x = std::max(room.left() + 4, std::min(avoid.left(), room.right() - w - 4));
    const int y = std::max(room.top() + 4, std::min(avoid.top(), room.bottom() - h - 4));

    const QRect tries[] = {
        QRect(x, avoid.bottom() + kOffset, w, h),   // dessous
        QRect(x, avoid.top() - h - kOffset, w, h),  // dessus
        QRect(avoid.right() + kOffset, y, w, h),    // à droite
        QRect(avoid.left() - w - kOffset, y, w, h), // à gauche
    };
    for (const QRect &box : tries) {
        if (!room.contains(box)) continue;
        if (box.intersects(avoid) || box.contains(mouse)) continue;
        return box;
    }
    return QRect();
}

} // namespace

RivalGlance::RivalGlance(const QString &rivalId, QWidget *about)
    : QWidget(about, Qt::ToolTip | Qt::FramelessWindowHint), m_rivalId(rivalId) {
    m_hue = rivalById(rivalId).hue;

    // L'aperçu ne capte aucun événement : ni clic, ni focus, ni survol. À
    // défaut, un aperçu placé sous le pointeur ferait sortir celui-ci de la
    // carte l'ayant demandé, et l'aperçu se refermerait aussitôt.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);

    auto *columns = new QHBoxLayout(this);
    columns->setContentsMargins(kPad, kPad, kPad, kPad);
    columns->setSpacing(kBetween);

    auto *left = new QVBoxLayout;
    left->setSpacing(kTight);
    auto *right = new QVBoxLayout;
    right->setSpacing(kTight);
    columns->addLayout(left);
    columns->addLayout(right);

    buildWhoHeIs(left);
    buildWhatHeLearned(right);
}

void RivalGlance::buildWhoHeIs(QVBoxLayout *column) {
    const Rival &who = rivalById(m_rivalId);

    column->addWidget(dialogSectionHeading(this, tr("Ce qu'il est")));
    column->addSpacing(5);
    // Sa petite phrase, qui est sa façon de jouer en une ligne, puis le niveau
    // avec lequel il s'est assis. Les deux ne bougent jamais : c'est ce qui
    // distingue cette colonne de l'autre.
    column->addWidget(glanceLine(this, who.blurb, false, kBlurbWide));
    column->addSpacing(4);
    column->addWidget(
        glanceLine(this, tr("Arrivé %1").arg(levelName(levelOf(who.startXp)).toLower()), true));
    column->addStretch(1);
}

void RivalGlance::buildWhatHeLearned(QVBoxLayout *column) {
    const Learning learning = learningFor(m_rivalId);
    const QColor mine = rivalColour(m_hue);

    column->addWidget(dialogSectionHeading(this, tr("Ce qu'il a appris")));
    column->addSpacing(5);
    column->addWidget(new GlanceLevelLine(learning.xp, mine, this));
    column->addWidget(glanceLine(this, levelName(levelOf(learning.xp)), false));
    column->addSpacing(4);

    if (learning.donnesPlayed == 0) {
        // Un joueur qui n'a jamais joué le dit, parce que c'est la réponse à
        // « pourquoi est-il encore débutant ».
        column->addWidget(glanceLine(this, tr("Pas encore joué sous ce profil"), true));
        column->addStretch(1);
        return;
    }

    // Trois accords à faire à la main. Le pluriel de Qt demande un fichier de
    // traduction, et il n'y en a pas : le jeu est en français et en français
    // seulement.
    const QString donnes = learning.donnesPlayed > 1 ? tr("%1 donnes jouées, dont %2 gagnées")
                                                     : tr("%1 donne jouée, dont %2 gagnée");
    const QString contracts =
        learning.contractsMade > 1 ? tr("%1 contrats tenus") : tr("%1 contrat tenu");
    const QString matches = learning.matchesPlayed > 1 ? tr("%1 parties, dont %2 gagnées")
                                                       : tr("%1 partie, dont %2 gagnée");

    column->addWidget(
        glanceLine(this, donnes.arg(learning.donnesPlayed).arg(learning.donnesWon), true));
    column->addWidget(glanceLine(this, contracts.arg(learning.contractsMade), true));
    if (learning.matchesPlayed > 0) {
        column->addWidget(
            glanceLine(this, matches.arg(learning.matchesPlayed).arg(learning.matchesWon), true));
    }
    column->addStretch(1);
}

void RivalGlance::watch(const QRect &avoid) {
    m_avoid = avoid;
    if (!m_watch) {
        m_watch = new QTimer(this);
        m_watch->setInterval(kWatchMs);
        connect(m_watch, &QTimer::timeout, this, [this] {
            // Un aperçu qui n'est plus l'aperçu courant est déjà en cours de
            // fermeture : il ne doit pas fermer celui qui l'a remplacé.
            if (!wanted() && g_glance == this) hideRivalGlance();
        });
    }
    m_watch->start();
}

bool RivalGlance::wanted() const {
    const QPoint mouse = QCursor::pos();
    return m_avoid.contains(mouse) || frameGeometry().contains(mouse) ||
           geometry().contains(mouse);
}

void RivalGlance::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    const QColor mine = rivalColour(m_hue);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Le panneau, bordé de la couleur de l'adversaire décrit : affiché sur le
    // tapis à côté de deux autres plaques, c'est ce qui l'identifie sans
    // répéter son nom.
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(blendColours(scheme.edge, mine, 0.55), 1.0));
    painter.setBrush(scheme.surface);
    painter.drawRoundedRect(box, kCorner, kCorner);
}

void showRivalGlance(const QString &rivalId, const QRect &avoid, QWidget *about) {
    if (!about || rivalId.isEmpty()) return;
    // Un aperçu déjà affiché est conservé : il n'est pas reconstruit pour le
    // même adversaire à la même position, la reconstruction étant visible.
    if (g_glance && g_about == about && g_glance->rivalId() == rivalId) {
        g_glance->watch(avoid);
        return;
    }
    hideRivalGlance();

    // Reconstruit à chaque affichage plutôt que conservé : il reflète
    // l'expérience et les couleurs courantes, qu'une donne jouée entre deux
    // survols peut avoir modifiées.
    auto *glance = new RivalGlance(rivalId, about);
    glance->adjustSize();
    const QSize size = glance->size();

    const QScreen *screen = about->screen() ? about->screen() : QGuiApplication::primaryScreen();
    const QRect room = screen ? screen->availableGeometry() : avoid;

    QRect place = glancePlace(size, avoid, room, QCursor::pos());
    if (place.isNull()) {
        // Aucune position ne permet d'éviter tout recouvrement : l'aperçu est
        // placé en dessous et ramené de force dans l'écran. Le pointeur peut
        // alors passer dessus, l'aperçu assurant désormais lui-même la
        // surveillance et ne se fermant plus sur un événement de sortie.
        const int x =
            std::max(room.left() + 4, std::min(avoid.left(), room.right() - size.width() - 4));
        const int y = std::max(
            room.top() + 4,
            std::min(avoid.bottom() + kOffset, room.bottom() - size.height() - 4));
        place = QRect(QPoint(x, y), size);
    }

    glance->move(place.topLeft());
    glance->show();
    glance->watch(avoid);
    g_glance = glance;
    g_about = about;
}

void hideRivalGlance(const QWidget *about) {
    if (about && g_about && g_about != about) return;
    if (g_glance) {
        g_glance->hide();
        g_glance->deleteLater();
    }
    g_glance = nullptr;
    g_about = nullptr;
}

void releaseRivalGlance(const QWidget *about) {
    if (!g_glance) return;
    if (about && g_about && g_about != about) return;
    // Le pointeur est sorti de la carte, ou bien l'aperçu vient de s'afficher
    // par-dessus, ce qui produit le même événement. L'aperçu est conservé tant
    // que le pointeur se trouve sur lui ; sa propre surveillance assurera sa
    // fermeture.
    if (g_glance->wanted()) return;
    hideRivalGlance();
}
