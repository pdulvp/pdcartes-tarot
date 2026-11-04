#include "TableView.h"

#include "Poignee.h"
#include "Tarot.h"
#include "FeltArt.h"
#include "Fonts.h"
#include "CardDeck.h"
#include "TableLook.h"
#include "Palette.h"
#include "RivalGlance.h"
#include "Rivals.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QStringList>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

constexpr qreal kPi = 3.14159265358979323846;
constexpr int kFrameMs = 16;

// Ce que chaque bande du drap reçoit, avant que rien ne soit serré.
constexpr qreal kSeatsHeight = 118.0;
constexpr qreal kSideWidth = 192.0;
constexpr qreal kRailHeight = 54.0;
constexpr qreal kHandHeight = 178.0;
constexpr qreal kMargin = 14.0;
constexpr qreal kGap = 10.0;

// Les corps de texte d'une plaque, et la hauteur utile où ils valent. Ce
// sont ceux de la carte du même joueur dans le menu d'ouverture
// (`RivalCard`) : c'est la même personne décrite de la même manière, et la
// lire plus gros à la table qu'au menu la fait passer pour deux objets.
constexpr int kPlaqueNameSize = 15;
constexpr int kPlaqueAsideSize = 11;
constexpr int kPlaqueBadgeSize = 10;
// Ce qui reste d'une plaque quand on lui a retiré ses marges du haut et du
// bas, la fenêtre lui laissant toute sa place.
constexpr qreal kPlaqueInnerFull = kSeatsHeight - 14.0;

// Le corps d'un texte de plaque, à la hauteur qu'elle a. La rangée des
// plaques se réduit avec la fenêtre (`min(kSeatsHeight, hauteur * 0.21)`), et
// un corps fixe y déborderait : le texte garde donc sa réduction. Ce qu'il
// n'a plus, c'est le droit de grandir -- la taille du menu est un plafond, et
// non une valeur de départ qu'une plaque entière multipliait par un cinquième.
int plaqueTextSize(qreal innerHeight, int atFullHeight, int floor) {
    const qreal scale = std::min(1.0, innerHeight / kPlaqueInnerFull);
    return std::max(floor, int(std::lround(atFullHeight * scale)));
}

// La plus large qu'une plaque soit dessinée, et la plus étroite qu'on accepte
// de la faire pour la poser sur le côté : en dessous, le nom et « contre
// vous » ne tiennent plus l'un à côté de l'autre.
constexpr qreal kWidestPlaque = 268.0;
constexpr qreal kAsideWidth = 222.0;

// Ce qu'il faut laisser au pli entre deux plaques de côté : trois cartes et un
// peu, puisque la croix fait une carte et deux écarts de large.
constexpr qreal kCrossRoom = 300.0;

// Et ce que la table mesure quand la fenêtre est plus grande qu'elle.
//
// Une table de tarot ne s'élargit pas parce que la pièce est grande : les
// quatre joueurs restent à portée de bras les uns des autres. Au-delà d'une
// certaine largeur, le drap continue donc tout seul et c'est la table qui est
// centrée dessus, plaques comprises -- sans quoi les deux adversaires
// finissent collés aux bords d'un champ vert avec quatre petites cartes
// perdues au milieu.
constexpr qreal kCrossPerCard = 4.6;

// De combien une carte choisie se soulève de la main.
constexpr qreal kLift = 18.0;

// La taille des cartes, et de combien un cran de molette la change.
constexpr int kZoomMin = 80;
constexpr int kZoomMax = 200;
constexpr int kZoomStep = 10;

// L'allure ordinaire d'une carte qui vole, et celle d'une carte qu'on donne.
constexpr qreal kFlightSpeed = 0.068;
constexpr qreal kDealSpeed = 0.085;

// Ce qu'une carte donnée attend que la précédente ait pris d'avance, en part
// du trajet.
//
// Une donne met soixante-douze cartes en l'air -- dix-huit pour chacun -- et
// soixante-douze volées parties ensemble ne se lisent pas : on voit les quatre
// mains se remplir sans voir personne donner. L'une après l'autre, on suit.
constexpr qreal kDealStagger = 0.22;

// Et la distribution entière tient dans ce temps-là, quel que soit le nombre
// de cartes qu'elle met en l'air. À 0,085 par image, 4,5 fait un peu moins
// d'une seconde entre la première carte et la dernière -- soixante-douze
// cartes données une par une à leur rythme propre dureraient une minute.
constexpr qreal kDealSpread = 4.5;

// Votre carte, seule, prend le temps de se retourner -- et ce temps-là suit la
// distance qu'elle a à couvrir : une carte qui traverse le drap pour arriver en
// main ne peut pas faire son demi-tour en trois images.
qreal flipSpeedFor(qreal distance, qreal across) {
    const qreal part = across > 1.0 ? std::clamp(distance / across, 0.0, 1.0) : 0.5;
    return 0.055 - 0.020 * part;
}

qreal plateau(qreal along) {
    // Part vite et se pose doucement : une carte lancée sur une table
    // n'arrive pas à vitesse constante.
    const qreal at = std::clamp(along, 0.0, 1.0);
    return 1.0 - (1.0 - at) * (1.0 - at);
}

// Le corps d'une plaque -- celle d'un joueur d'en face, et la vôtre.
//
// Sombre et translucide à filet d'or, comme les cartes du menu d'ouverture :
// un panneau blanc sur un drap vert est un trou dans le drap, et les deux
// pages doivent se ressembler. Celui dont c'est le tour s'éclaire un peu et
// prend sa propre couleur dans le filet -- c'est la seule chose qui distingue
// encore les quatre d'un coup d'œil.
void paintPlaqueBody(QPainter *painter, const QRectF &box, const Palette &scheme,
                     const QColor &mine, bool active) {
    QPainterPath shape;
    shape.addRoundedRect(box, 11, 11);

    painter->setPen(Qt::NoPen);
    QColor body = feltPlaqueFill(scheme, active ? 0.10 : 0.0);
    if (active) body = blendColours(body, mine, 0.14);
    painter->fillPath(shape, body);

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(active ? mine : feltGold(scheme, 0.26), active ? 2.4 : 1.0));
    painter->drawPath(shape);
}

// L'encre d'une plaque, et son aparté : claires, maintenant qu'elle est
// sombre.
QColor plaqueInk(const Palette &scheme) { return scheme.feltText; }

QColor plaqueAside(const Palette &scheme) {
    QColor ink = scheme.feltText;
    ink.setAlphaF(0.62);
    return ink;
}

// Les enseignes du tapis -- « L'atout », ce qui nomme un endroit plutôt qu'un
// détail -- dans la fonte du haut du logo, comme les titres de section du
// menu. Elle n'a pas de bas-de-casse, ses minuscules étant ses capitales :
// `AllUppercase` ne sert donc qu'au repli.
QFont capsFont(const QPainter *painter, int size) {
    QFont font = familyFont(plainFont(painter), capsFamily(), size + 2);
    font.setCapitalization(QFont::AllUppercase);
    font.setLetterSpacing(QFont::PercentageSpacing, 120);
    return font;
}

} // namespace

TableView::TableView(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(780, 540);
    setAutoFillBackground(false);

    m_ticker = new QTimer(this);
    m_ticker->setInterval(kFrameMs);
    connect(m_ticker, &QTimer::timeout, this, &TableView::tick);

    // Aperçu d'un adversaire, affiché pendant le survol de sa plaque : sa
    // façon de jouer et son expérience acquise, consultables sans quitter la
    // donne en cours.
    m_glanceBeat = new QTimer(this);
    m_glanceBeat->setSingleShot(true);
    m_glanceBeat->setInterval(kGlanceDelayMs);
    connect(m_glanceBeat, &QTimer::timeout, this, [this] {
        if (!m_game || m_hoveredSeat < 0) return;
        const QString who = m_game->seat(m_hoveredSeat).rivalId;
        if (who.isEmpty()) return;
        for (const Spot &spot : m_seatBoxes) {
            if (spot.id != m_hoveredSeat) continue;
            // Position de la plaque en coordonnées d'écran : l'aperçu est
            // affiché en dessous, sur le tapis, où il ne masque ni la plaque
            // ni la main du joueur.
            showRivalGlance(who,
                            QRect(mapToGlobal(spot.box.topLeft().toPoint()),
                                  spot.box.size().toSize()),
                            this);
            return;
        }
    });
}

void TableView::watch(const Tarot *game) {
    m_game = game;
    m_chosen = -1;
    m_picked.clear();
    m_flights.clear();
    // Les places d'avant appartenaient à une autre partie. Les cartes portent
    // les mêmes numéros d'une donne à l'autre, alors les garder ferait glisser
    // la main neuve depuis la main d'hier.
    m_handWas.clear();
    m_slideFrom.clear();
    m_slideAlong = 1.0;
    relayout();
    update();
}

void TableView::setChosen(int cardId) {
    if (m_chosen == cardId) return;
    m_chosen = cardId;
    update();
}

void TableView::clearChosen() { setChosen(-1); }

void TableView::togglePicked(int cardId) {
    if (m_picked.contains(cardId)) {
        m_picked.remove(cardId);
    } else {
        m_picked.insert(cardId);
    }
    update();
}

void TableView::clearPicked() {
    if (m_picked.isEmpty()) return;
    m_picked.clear();
    update();
}

void TableView::say(const QString &line) {
    m_line = line;
    m_trouble = false;
    update();
}

void TableView::sayTrouble(const QString &line) {
    m_line = line;
    m_trouble = true;
    update();
}

void TableView::refresh() {
    relayout();
    update();
}

int TableView::mySeat() const {
    if (!m_game) return -1;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) return at;
    }
    return -1;
}

int TableView::compassOf(int at) const {
    const int me = mySeat();
    if (me < 0 || at < 0 || !m_game) return 0;
    const int seats = m_game->seatCount();
    return ((at - me) % seats + seats) % seats;
}

bool TableView::myTurn() const {
    if (!m_game) return false;
    return m_game->phase() != Phase::Idle && m_game->turn() == mySeat() &&
           (m_game->phase() == Phase::Playing || m_game->phase() == Phase::Bidding);
}

QPointF TableView::seatAnchor(int at) const {
    for (const Spot &spot : m_seatBoxes) {
        if (spot.id == at) return spot.box.center();
    }
    // La vôtre part de votre main et y revient.
    return QPointF(m_handRow.center().x(), m_handRow.center().y());
}

void TableView::cardsOnScreen(QVector<Card> *cards, QVector<QPointF> *at) const {
    if (!cards || !at || !m_game) return;
    // À la fin d'une donne les trente-deux cartes sont ramassées en piles au
    // milieu du drap : c'est là qu'elles sont, et c'est de là qu'elles
    // s'envolent.
    const QPointF middle = m_cross.isNull() ? QPointF(width() / 2.0, height() / 2.0)
                                            : m_cross.center();
    int i = 0;
    for (const Played &played : m_game->history()) {
        const qreal spread = 0.6 * i;
        cards->append(played.card);
        at->append(middle + QPointF(std::cos(i * 2.4) * spread, std::sin(i * 2.4) * spread));
        ++i;
    }
}

bool TableView::flying(int cardId) const {
    for (const Flight &flight : m_flights) {
        if (flight.card.id == cardId) return true;
    }
    return false;
}

void TableView::stopAnimations() {
    m_flights.clear();
    m_handWas.clear();
    m_slideFrom.clear();
    m_slideAlong = 1.0;
    m_ticker->stop();
    update();
}

void TableView::wake() {
    if (!m_ticker->isActive()) m_ticker->start();
}

void TableView::notePlayed(int seat, const Card &card) {
    if (!card.valid() || seat < 0) return;
    const QRectF landing = m_slotBoxes[compassOf(seat)];
    if (landing.isNull()) return;

    // La carte vole de la plaque jusqu'à sa place dans le pli, et c'est tout
    // ce qu'il faut pour voir qui vient de jouer : elle part de chez lui et
    // elle se pose de son côté. Rien ne reste allumé après, parce que le
    // liseré doré du pli veut dire « c'est elle qui tient le pli » et qu'une
    // seconde chose dorée sur la table en ferait une devinette.
    Flight flight;
    flight.card = card;
    flight.faceUp = true;
    flight.size = landing.size();
    flight.to = landing.topLeft();
    // Une carte qui est à vous part de la place qu'elle occupait dans la main,
    // et non de votre plaque : c'est celle-là qu'on vient de cliquer, et c'est
    // de là qu'on l'a vue partir.
    const QRectF leaves = m_handWas.value(card.id);
    if (m_game && m_game->seat(seat).human() && !leaves.isNull()) {
        flight.from = leaves.topLeft();
        flight.size = leaves.size();
        flight.grow = landing.size();
    } else {
        flight.from =
            seatAnchor(seat) - QPointF(landing.width() / 2.0, landing.height() / 2.0);
    }
    flight.speed = kFlightSpeed;
    m_flights.append(flight);
    wake();
}

QRectF TableView::dealBox() const {
    // Le milieu du drap : là où le talon serait si on le dessinait, et donc
    // l'endroit d'où une carte donnée vient.
    const QSizeF card = cardSizeForHeight(m_trickHeight);
    QRectF box(0, 0, card.width(), card.height());
    box.moveCenter(m_cross.center());
    return box;
}

void TableView::noteDealt() {
    if (!m_game || m_game->seatCount() < kMinSeats) return;

    // Les places d'abord, parce que c'est là qu'on va chercher où chaque carte
    // de votre main atterrit.
    relayout();
    // La main vient d'être distribuée : elle n'était nulle part avant, et la
    // faire glisser depuis une place d'avant la ferait venir de la donne
    // précédente.
    m_slideFrom.clear();
    m_slideAlong = 1.0;

    const QRectF from = dealBox();
    const int me = mySeat();
    int dealt = 0;
    for (int at = 0; at < m_game->seatCount(); ++at) dealt += m_game->seat(at).hand.size();
    const qreal stagger = std::min(kDealStagger, kDealSpread / std::max(1, dealt));
    // Dans l'ordre où l'on donne : trois à chacun en partant de la gauche du
    // donneur, puis deux, ce que la vue n'a pas à savoir -- elle suit l'ordre
    // dans lequel les cartes sont arrivées en main.
    int rank = 0;
    const int seats = m_game->seatCount();
    for (int step = 1; step <= seats; ++step) {
        const int at = (m_game->dealer() + step) % seats;
        const Seat &seat = m_game->seat(at);
        for (const Card &card : seat.hand) {
            Flight flight;
            flight.card = card;
            flight.from = from.topLeft();
            flight.size = from.size();
            flight.speed = kDealSpeed;
            // Une carte à vous se retourne en arrivant ; celle des autres
            // reste de dos, ce qui est tout ce qu'on en voit.
            if (at == me) {
                QRectF landing;
                for (const Spot &spot : m_handBoxes) {
                    if (spot.id == card.id) landing = spot.box;
                }
                if (landing.isNull()) continue;
                flight.flips = true;
                flight.faceUp = true;
                flight.to = landing.topLeft();
                flight.grow = landing.size();
            } else {
                flight.faceUp = false;
                flight.to =
                    seatAnchor(at) - QPointF(from.width() / 2.0, from.height() / 2.0);
            }
            flight.along = -stagger * qreal(rank);
            ++rank;
            m_flights.append(flight);
        }
    }
    wake();
}

void TableView::noteChienTaken(const QVector<int> &cardIds) {
    if (!m_game || cardIds.isEmpty() || m_game->seatCount() < kMinSeats) return;

    relayout();

    const int me = mySeat();
    const int taker = m_game->taker();
    if (taker < 0) return;

    int rank = 0;
    for (int id : cardIds) {
        const Card *landed = findCard(m_game->seat(taker).hand, id);
        if (!landed) continue;

        // Chaque carte part de la place qu'elle occupait dans le rang du
        // chien, au milieu du drap : la table vient de les y lire, et c'est de
        // là qu'on les voit s'en aller chez le preneur. Elles ne se retournent
        // pas en chemin -- tout le monde les a vues.
        const QRectF from =
            rank < m_chienBoxes.size() ? m_chienBoxes.at(rank) : dealBox();

        Flight flight;
        flight.card = *landed;
        flight.from = from.topLeft();
        flight.size = from.size();
        flight.faceUp = true;
        flight.speed = kFlightSpeed;

        if (taker == me) {
            QRectF landing;
            for (const Spot &spot : m_handBoxes) {
                if (spot.id == id) landing = spot.box;
            }
            if (landing.isNull()) continue;
            flight.to = landing.topLeft();
            flight.grow = landing.size();
        } else {
            flight.to = seatAnchor(taker) - QPointF(from.width() / 2.0, from.height() / 2.0);
        }

        // Négatif : chacune attend que la précédente soit partie.
        flight.along = -kDealStagger * qreal(rank);
        ++rank;
        m_flights.append(flight);
    }
    wake();
}

void TableView::noteEcart(const QVector<int> &cardIds) {
    if (!m_game || cardIds.isEmpty()) return;

    const int taker = m_game->taker();
    if (taker < 0) return;

    // L'écart s'en va de dos : six cartes que personne ne reverra avant la
    // fin de la donne, et que les défenseurs n'ont pas le droit de voir.
    const QRectF talon = dealBox();
    int rank = 0;
    for (int id : cardIds) {
        const QRectF leaves = m_handWas.value(id);
        Flight flight;
        flight.card = Card{};
        flight.faceUp = false;
        flight.from = (leaves.isNull() ? talon : leaves).topLeft();
        flight.size = (leaves.isNull() ? talon : leaves).size();
        flight.to = seatAnchor(taker) - QPointF(flight.size.width() / 2.0,
                                                flight.size.height() / 2.0);
        flight.grow = talon.size();
        flight.speed = kFlightSpeed;
        flight.along = -kDealStagger * qreal(rank);
        ++rank;
        m_flights.append(flight);
    }
    m_picked.clear();
    wake();
}

void TableView::noteTrickTaken(int winner) {
    if (!m_game) return;
    const QPointF to = seatAnchor(winner);
    for (const Played &played : m_game->lastTrick()) {
        const QRectF from = m_slotBoxes[compassOf(played.seat)];
        if (from.isNull()) continue;
        Flight flight;
        flight.card = played.card;
        flight.faceUp = true;
        flight.size = from.size();
        flight.from = from.topLeft();
        flight.to = to - QPointF(from.width() / 2.0, from.height() / 2.0);
        flight.speed = 0.075;
        m_flights.append(flight);
    }
    wake();
}

void TableView::tick() {
    bool busy = false;

    for (Flight &flight : m_flights) {
        flight.along += flight.speed;
        if (flight.along < 1.0) busy = true;
    }
    m_flights.erase(std::remove_if(m_flights.begin(), m_flights.end(),
                                   [](const Flight &flight) { return flight.along >= 1.0; }),
                    m_flights.end());

    // L'écartement de la main. Il avance seul : ranger sa main après une prise
    // ne fait pas forcément voler de carte, et sans cela la main resterait
    // dessinée là où elle était pendant que le pointage viserait déjà les
    // nouvelles places.
    if (m_slideAlong < 1.0) {
        m_slideAlong = std::min(1.0, m_slideAlong + 0.11);
        if (m_slideAlong < 1.0) busy = true;
        else m_slideFrom.clear();
    }

    update();
    if (!busy) m_ticker->stop();
}

void TableView::setZoom(int percent) {
    const int now = qBound(kZoomMin, percent, kZoomMax);
    if (now == m_zoom) return;
    m_zoom = now;
    // Les images mises à l'échelle sont gardées par taille : en changer toutes
    // les tailles d'un coup laisse derrière soi un cache qui ne resservira
    // plus.
    if (const CardSet *set = CardDecks::instance().current()) set->forgetDrawings();
    forgetBackDrawings();
    relayout();
    update();
    Q_EMIT zoomChanged(now);
}

// ---------------------------------------------------------------------------
// Où tout se place.

void TableView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    relayout();
}

void TableView::relayout() {
    // Les places d'avant ne valent plus rien si le drap change de forme.
    if (m_slideAlong >= 1.0) m_slideFrom.clear();
    const QRectF all = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    if (all.width() < 80 || all.height() < 80) return;

    // Les deux bornes de chaque bande sont tirées par le zoom, pas seulement
    // la première : sans cela, à cent pour cent rien ne bouge -- c'est voulu --
    // mais au-delà la bande reste et la carte déborde.
    const qreal z = m_zoom / 100.0;
    const qreal seats = std::min(kSeatsHeight, all.height() * 0.21);
    const qreal hand = std::min(kHandHeight * z, all.height() * 0.26 * z);
    const qreal rail = std::min(kRailHeight * z, all.height() * 0.11 * z);

    m_seatsRow = QRectF(all.left(), all.top(), all.width(), seats);
    m_handRow = QRectF(all.left(), all.bottom() - hand, all.width(), hand);
    m_rail = QRectF(all.left(), m_handRow.top() - rail - kGap * 0.4, all.width(), rail);

    const qreal middleTop = m_seatsRow.bottom() + kGap;
    const qreal middleHeight = std::max(80.0, m_rail.top() - kGap - middleTop);
    const qreal side = std::min(kSideWidth, all.width() * 0.23);
    m_tapis = QRectF(all.left(), middleTop, all.width() - side - kGap, middleHeight);
    m_side = QRectF(m_tapis.right() + kGap, middleTop, side, middleHeight);

    // La taille des cartes du pli est décidée ici, avant les plaques : c'est
    // elle qui dit la largeur qu'il faut laisser à la croix, et donc où les
    // plaques de côté se posent.
    m_trickHeight = std::clamp(m_tapis.height() * 0.30, 54.0 * z, 136.0 * z);

    layoutSeats();
    layoutSlots();
    layoutHand();
}

bool TableView::roomAround(qreal plaque) const {
    // Ce qui reste entre les deux plaques de côté doit tenir une croix de
    // cartes, et une croix fait trois cartes et un peu de large.
    return m_tapis.width() - 2.0 * (plaque + kGap) >= kCrossRoom;
}

void TableView::layoutSeats() {
    m_seatBoxes.clear();
    m_cross = m_tapis;
    m_aroundTable = false;
    if (!m_game || m_game->seatCount() < kMinSeats) return;

    const int me = mySeat();
    if (me < 0) return;
    // Les autres, dans l'ordre où ils sont assis autour de vous. Vos propres
    // cartes sont devant vous et votre marque est dans la colonne, où il y a
    // la place.
    const int seats = m_game->seatCount();
    QVector<int> others;
    for (int step = 1; step < seats; ++step) others.append((me + step) % seats);

    const qreal aside = std::clamp(m_tapis.width() * 0.25, kAsideWidth, kWidestPlaque);
    // À 4 joueurs, les plaques vont autour du tapis quand la place le permet.
    // À 3 ou 5 joueurs, elles restent en bande au-dessus, comme à card-sol :
    // il n'y a pas de croix à trois ou cinq places où la plaque dise le côté
    // de la carte.
    m_aroundTable = seats == kSeatCount && roomAround(aside);

    if (!m_aroundTable) {
        // En bande au-dessus du tapis, les trois côte à côte et dans l'ordre
        // où ils sont assis : la gauche à gauche, le partenaire au milieu.
        qreal width = (m_seatsRow.width() - kGap * (others.size() - 1)) / others.size();
        width = std::min(width, kWidestPlaque);
        const qreal span = width * others.size() + kGap * (others.size() - 1);
        qreal at = m_seatsRow.left() + (m_seatsRow.width() - span) / 2.0;
        for (int seat : others) {
            m_seatBoxes.append({seat, QRectF(at, m_seatsRow.top(), width, m_seatsRow.height())});
            at += width + kGap;
        }
        return;
    }

    // Chacun à sa place. Le partenaire au milieu de la bande du haut, les deux
    // adversaires à gauche et à droite du pli et à mi-hauteur -- c'est-à-dire
    // là où leur carte va tomber, ce qui fait que la plaque et le pli disent
    // la même chose.
    //
    // La table est aussi large qu'il faut et pas plus, puis centrée sur le
    // drap : les deux adversaires restent à côté du pli au lieu de s'éloigner
    // avec les bords de la fenêtre.
    const qreal height = m_seatsRow.height();
    const qreal middle = m_tapis.center().y();
    const qreal cross =
        std::max(kCrossRoom, cardSizeForHeight(m_trickHeight).width() * kCrossPerCard);
    const qreal band = std::min(m_tapis.width(), 2.0 * (aside + kGap) + cross);
    const qreal left = m_tapis.left() + (m_tapis.width() - band) / 2.0;

    QRectF partner(0, 0, std::min(kWidestPlaque, band), height);
    partner.moveTopLeft(QPointF(left + (band - partner.width()) / 2.0, m_seatsRow.top()));
    m_seatBoxes.append({others.at(1), partner});

    m_seatBoxes.append({others.at(0), QRectF(left, middle - height / 2.0, aside, height)});
    m_seatBoxes.append(
        {others.at(2), QRectF(left + band - aside, middle - height / 2.0, aside, height)});

    m_cross = QRectF(left + aside + kGap, m_tapis.top(), band - 2.0 * (aside + kGap),
                     m_tapis.height());
}

void TableView::layoutSlots() {
    const qreal height = m_trickHeight;
    const QSizeF card = cardSizeForHeight(height);
    const QPointF middle = m_cross.center();

    // Les places du pli, en cercle autour du milieu : chacune est du côté de
    // celui qui y pose sa carte, et c'est ce qui fait qu'on sait de qui est la
    // carte sans qu'on l'écrive. À 4 joueurs le cercle est la croix usuelle ;
    // à 3 ou 5 joueurs il devient un triangle ou un pentagone aplati.
    //
    // L'écart de gauche à droite fait plus d'une carte de large, ce qui suffit
    // à lui seul : deux cartes qui ne se chevauchent pas en largeur ne se
    // chevauchent pas, quelle que soit leur hauteur. L'écart de haut en bas
    // n'a donc qu'à être assez grand pour que la croix soit une croix.
    const int seats = m_game ? m_game->seatCount() : kSeatCount;
    const qreal dx = card.width() * (seats > 4 ? 1.30 : 1.14);
    const qreal dy = card.height() * (seats > 4 ? 0.52 : 0.60);
    m_slotBoxes.clear();
    for (int i = 0; i < seats; ++i) {
        // La place 0 est en bas (la vôtre) ; on tourne dans le sens du jeu.
        const qreal angle = kPi / 2.0 + i * 2.0 * kPi / seats;
        QRectF box(0, 0, card.width(), card.height());
        box.moveCenter(QPointF(middle.x() + dx * std::cos(angle), middle.y() + dy * std::sin(angle)));
        m_slotBoxes.append(box);
    }

    // Le chien, posé en rang au milieu du drap le temps qu'on le lise. Six
    // cartes côte à côte, un peu plus grandes que celles du pli : c'est la
    // seule chose sur la table à ce moment-là, et elle décide de la donne.
    m_chienBoxes.clear();
    const QSizeF big = cardSizeForHeight(std::min(height * 1.10, m_cross.height() * 0.42));
    const int chien = m_game ? m_game->chienSize() : 6;
    const qreal step = std::min(big.width() + 8.0, (m_cross.width() - 24.0) / qreal(chien));
    const qreal span = big.width() + step * (chien - 1);
    const qreal left = middle.x() - span / 2.0;
    for (int i = 0; i < chien; ++i) {
        m_chienBoxes.append(QRectF(left + step * i, middle.y() - big.height() / 2.0,
                                   big.width(), big.height()));
    }
}

QRectF TableView::handSlot(int index, int count) const {
    if (count <= 0 || index < 0 || index >= count) return QRectF();
    const qreal z = m_zoom / 100.0;
    // Les cartes de la main suivent celles du pli : une main dessinée deux
    // fois plus grande que le pli qu'elle dispute n'est plus une main, c'est
    // un éventail posé devant une maquette.
    const qreal height =
        std::min(m_handRow.height() - kLift - 6.0, std::max(124.0 * z, m_trickHeight * 1.22));
    const QSizeF card = cardSizeForHeight(std::max(54.0, height));
    const qreal step = count > 1 ? std::min(card.width() + 9.0,
                                            (m_handRow.width() - card.width()) / (count - 1))
                                 : 0.0;
    const qreal span = card.width() + step * (count - 1);
    const qreal left = m_handRow.left() + std::max(0.0, (m_handRow.width() - span) / 2.0);
    const qreal top = m_handRow.bottom() - card.height();
    return QRectF(left + step * index, top, card.width(), card.height());
}

QRectF TableView::handBoxNow(int id, const QRectF &box) const {
    const auto slid = m_slideFrom.constFind(id);
    if (slid == m_slideFrom.constEnd()) return box;
    QRectF now = box;
    now.moveLeft(slid->left() + (box.left() - slid->left()) * plateau(m_slideAlong));
    return now;
}

void TableView::layoutHand() {
    m_handBoxes.clear();
    const int me = mySeat();
    if (!m_game || me < 0) return;

    const Seat &mine = m_game->seat(me);
    const int count = mine.hand.size();
    if (count <= 0) return;

    // D'où chaque carte part, si elle bouge.
    //
    // Plutôt qu'un cas par coup -- trois places qui s'ouvrent pour la prise,
    // une qui se referme sur la carte jouée, la main entière qui se range
    // quand l'atout est connu -- la main se compare simplement à ce qu'elle
    // était : toute carte qui change de place y glisse. Un coup qui la remanie
    // est alors animé sans que rien ici ait à savoir lequel c'était.
    //
    // Sauf quand c'est la fenêtre qui a changé de taille : là, les cartes sont
    // à leur place, c'est la place qui a bougé, et les faire glisser à chaque
    // pixel de la poignée ne ferait que traîner derrière la souris.
    const bool sameRow = m_handRow == m_handRowWas;
    QHash<int, QRectF> moving;
    for (int i = 0; i < count; ++i) {
        const int id = mine.hand.at(i).id;
        const QRectF box = handSlot(i, count);
        m_handBoxes.append({id, box});
        const auto before = m_handWas.constFind(id);
        if (sameRow && before != m_handWas.constEnd() &&
            std::abs(before->left() - box.left()) > 0.5) {
            moving.insert(id, *before);
        }
        m_handWas.insert(id, box);
    }
    m_handRowWas = m_handRow;
    if (!moving.isEmpty()) {
        m_slideFrom = moving;
        m_slideAlong = 0.0;
        wake();
    }
}

// ---------------------------------------------------------------------------
// Le dessin.

void TableView::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    drawFelt(&painter, QRectF(rect()), scheme);
    if (!m_game || m_game->seatCount() < kMinSeats) return;

    paintSeats(&painter);
    paintTapis(&painter);
    paintSide(&painter);
    paintRail(&painter);
    paintHand(&painter);
    paintFlights(&painter);
}

void TableView::paintSeatPlaque(QPainter *painter, const QRectF &box, int at) {
    const Palette &scheme = currentPalette();
    const Seat &seat = m_game->seat(at);
    const QColor mine = rivalColour(seat.hue);
    const bool playing = m_game->phase() == Phase::Playing ||
                         m_game->phase() == Phase::Bidding || m_game->phase() == Phase::Ecart;
    const bool active = playing && m_game->turn() == at;
    const int takerSeat = m_game->taker();
    const bool taker = takerSeat == at;

    paintPlaqueBody(painter, box, scheme, mine, active);

    // Une plaque plus haute est une plaque plus aérée, et non une plaque
    // écrite plus gros : passé une certaine taille le nom déborderait sur
    // « contre vous » et les deux deviendraient illisibles ensemble.
    const QRectF inner = box.adjusted(9, 7, -9, -7);
    const qreal disc = std::min(inner.height() * 0.60, 42.0);
    const QRectF medallion(inner.left(), inner.top(), disc, disc);
    int emblem = 0;
    if (!seat.rivalId.isEmpty()) emblem = rivalById(seat.rivalId).emblem;
    drawRivalEmblem(painter, medallion, emblem, seat.name, mine,
                    blendColours(feltPlaqueFill(scheme), mine, 0.14));

    const qreal textLeft = medallion.right() + 9.0;
    const qreal textWidth = inner.right() - textLeft;

    // Preneur ou défenseur, écrit en clair dans le coin : au tarot les camps
    // se refont à chaque donne, et c'est la première chose à savoir d'un
    // joueur avant de poser une carte. Le preneur est seul contre les autres
    // — sauf à 5 joueurs, où le joueur appelé est avec lui : quand c'est vous
    // qui prenez, l'appelé est « avec vous » et les autres « contre vous » ;
    // quand c'est l'un d'eux, les autres sont « avec vous ». Posée avant le
    // nom, parce que c'est elle qui décide de la place qui reste au nom.
    QFont tag = plainFont(painter);
    tag.setBold(true);
    tag.setPixelSize(plaqueTextSize(inner.height(), kPlaqueBadgeSize, 9));
    tag.setCapitalization(QFont::AllUppercase);
    painter->setFont(tag);
    const QFontMetricsF tagMetrics(tag);
    const int callee = m_game->callee();
    const bool called = callee >= 0 && at == callee;
    const QString role = takerSeat < 0 ? tr("à l'enchère")
                         : taker       ? tr("preneur")
                         : called && takerSeat == mySeat() ? tr("avec vous")
                         : takerSeat == mySeat() ? tr("contre vous")
                                                 : tr("avec vous");
    QRectF chip(0, 0, tagMetrics.horizontalAdvance(role) + 12.0, tagMetrics.height() + 3.0);
    chip.moveTopRight(QPointF(inner.right(), inner.top() + 1.0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(taker ? scheme.gold : feltGold(scheme, 0.16));
    painter->drawRoundedRect(chip, chip.height() / 2.0, chip.height() / 2.0);
    painter->setPen(taker ? readableTextOn(scheme.gold) : plaqueAside(scheme));
    painter->drawText(capCentred(chip, tagMetrics), Qt::AlignCenter, role);

    QFont name = plainFont(painter);
    name.setBold(true);
    name.setPixelSize(plaqueTextSize(inner.height(), kPlaqueNameSize, 11));
    painter->setFont(name);
    painter->setPen(plaqueInk(scheme));
    const QFontMetricsF nameMetrics(name);
    // Jusqu'à la pastille et pas plus loin, et coupé s'il ne tient pas : un nom
    // qui déborde sur « contre vous » en fait deux illisibles au lieu d'un.
    const qreal nameRoom = std::max(24.0, chip.left() - 8.0 - textLeft);
    painter->drawText(QRectF(textLeft, inner.top(), nameRoom, nameMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(seat.name, Qt::ElideRight, nameRoom));

    qreal y = inner.top() + nameMetrics.height() + 2.0;
    if (!seat.rivalId.isEmpty()) {
        const Learning learning = learningFor(seat.rivalId);
        const Level level = levelOf(learning.xp);
        QFont small = plainFont(painter);
        small.setPixelSize(plaqueTextSize(inner.height(), kPlaqueAsideSize, 9));
        painter->setFont(small);
        const QFontMetricsF smallMetrics(small);

        const qreal starSpan = std::min(42.0, textWidth * 0.32);
        const QRectF starBox(textLeft, y + 1.0, starSpan, smallMetrics.height() - 2.0);
        drawLevelStars(painter, starBox, levelStars(level), 3, scheme.gold,
                       blendColours(feltPlaqueFill(scheme), plaqueInk(scheme), 0.16));
        painter->setPen(plaqueAside(scheme));
        // Le mot se centre sur les étoiles et non sur sa propre ligne : les
        // étoiles sont un dessin, haut de ce qu'on lui donne, et le texte
        // était posé plus bas qu'elles.
        QRectF levelBox(textLeft + starSpan + 6.0, 0.0, textWidth - starSpan - 6.0,
                        smallMetrics.height());
        levelBox.moveCenter(QPointF(levelBox.center().x(), starBox.center().y()));
        painter->drawText(capCentred(levelBox, smallMetrics), Qt::AlignLeft | Qt::AlignVCenter,
                          levelName(level));
        y += smallMetrics.height() + 1.0;

        drawGauge(painter, QRectF(textLeft, y + 2.0, std::min(textWidth, 92.0), 4.0),
                  progressWithin(learning.xp), mine,
                  blendColours(feltPlaqueFill(scheme), plaqueInk(scheme), 0.14));
        y += 9.0;
    }

    // Les cartes qui lui restent, de dos, et le compte dessus.
    const qreal pileHeight = std::max(20.0, inner.bottom() - y - 1.0);
    drawHandPile(painter,
                 QRectF(textLeft, inner.bottom() - pileHeight, std::max(50.0, textWidth * 0.60),
                        pileHeight),
                 seat.hand.size(), scheme, mine);

    // Le preneur porte son contrat sur sa plaque : c'est lui qui a quelque
    // chose à tenir, et c'est ce qui fait qu'on sait contre quoi on joue.
    qreal rightEdge = inner.right();
    if (taker) {
        QFont mark = plainFont(painter);
        mark.setBold(true);
        mark.setPixelSize(plaqueTextSize(inner.height(), kPlaqueAsideSize, 9));
        painter->setFont(mark);
        const QFontMetricsF markMetrics(mark);
        const QString shown = contractShort(m_game->contract());
        QRectF badge(0, 0, markMetrics.horizontalAdvance(shown) + 14.0,
                     markMetrics.height() + 3.0);
        badge.moveBottomRight(QPointF(rightEdge, inner.bottom()));
        painter->setPen(Qt::NoPen);
        painter->setBrush(blendColours(feltPlaqueFill(scheme), scheme.gold, 0.30));
        painter->drawRoundedRect(badge, badge.height() / 2.0, badge.height() / 2.0);
        painter->setPen(scheme.gold);
        painter->drawText(capCentred(badge, markMetrics), Qt::AlignCenter, shown);
        rightEdge = badge.left() - 8.0;
    }

    // Et la poignée qu'il a montrée. Une poignée passée dans la bande d'en bas
    // est une poignée oubliée, alors qu'elle dit où sont les atouts pour tout
    // le reste de la donne : elle reste donc écrite sur sa plaque.
    if (seat.poignee == PoigneeKind::Aucune) return;
    QFont mark = plainFont(painter);
    mark.setBold(true);
    mark.setPixelSize(plaqueTextSize(inner.height(), kPlaqueAsideSize, 9));
    painter->setFont(mark);
    const QFontMetricsF markMetrics(mark);
    const QString shown = countedText(seat.poigneeShown.size(), tr("atout"), tr("atouts"));
    QRectF said(0, 0, markMetrics.horizontalAdvance(shown) + 14.0, markMetrics.height() + 3.0);
    said.moveBottomRight(QPointF(rightEdge, inner.bottom()));
    painter->setPen(Qt::NoPen);
    painter->setBrush(scheme.gold);
    painter->drawRoundedRect(said, said.height() / 2.0, said.height() / 2.0);
    painter->setPen(readableTextOn(scheme.gold));
    painter->drawText(capCentred(said, markMetrics), Qt::AlignCenter, shown);
}

void TableView::paintSeats(QPainter *painter) {
    for (const Spot &spot : m_seatBoxes) paintSeatPlaque(painter, spot.box, spot.id);
}

void TableView::paintTapis(QPainter *painter) {
    const Palette &scheme = currentPalette();
    painter->setFont(capsFont(painter, 11));
    painter->setPen(scheme.feltText);
    QString heading;
    switch (m_game->phase()) {
    case Phase::Bidding: heading = tr("L'enchère"); break;
    case Phase::Chien: heading = tr("Le chien"); break;
    case Phase::Ecart: heading = tr("L'écart"); break;
    default:
        heading = tr("Le pli %1 sur %2")
                      .arg(std::max(1, m_game->trickNumber()))
                      .arg(m_game->trickCount());
        break;
    }
    // Dans le coin du tapis et non au-dessus : au-dessus, il n'y a que
    // l'écart entre deux bandes, et une fenêtre courte le réduit jusqu'à ce
    // que le titre vienne sur les plaques.
    painter->drawText(QRectF(m_tapis.left() + 8.0, m_tapis.top() + 2.0, m_tapis.width(), 14.0),
                      Qt::AlignLeft | Qt::AlignVCenter, heading);

    if (m_game->phase() == Phase::Bidding) {
        paintBidding(painter);
        return;
    }
    if (m_game->phase() == Phase::Chien || m_game->phase() == Phase::Ecart) {
        paintChien(painter);
        return;
    }
    if (m_game->phase() == Phase::Idle) return;
    paintTrick(painter);
}

void TableView::paintBidding(QPainter *painter) {
    const Palette &scheme = currentPalette();

    // Au tarot le chien reste couvert pendant l'enchère : il n'y a rien à
    // retourner, et ce qu'il y a au milieu de la table est la parole la plus
    // forte dite jusqu'ici.
    const bool spoken = m_game->bidder() >= 0;
    QFont big = plainFont(painter);
    big.setBold(true);
    big.setPixelSize(spoken ? 26 : 16);
    painter->setFont(big);
    const QFontMetricsF bigMetrics(big);
    const QString contract = spoken ? contractName(m_game->bid()) : tr("personne n'a parlé");

    QRectF badge(0, 0, bigMetrics.horizontalAdvance(contract) + 44.0,
                 bigMetrics.height() + 20.0);
    badge.moveCenter(m_cross.center());
    const QRectF heart = badge;
    painter->setPen(Qt::NoPen);
    painter->setBrush(blendColours(scheme.feltEdge, scheme.surface, 0.26));
    painter->drawRoundedRect(badge, 12, 12);
    painter->setPen(spoken ? scheme.gold : blendColours(scheme.feltText, scheme.felt, 0.3));
    painter->drawText(capCentred(badge, bigMetrics), Qt::AlignCenter, contract);

    painter->setFont(capsFont(painter, 11));
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_cross.left(), badge.top() - 20.0, m_cross.width(), 16.0),
                      Qt::AlignCenter, tr("Le contrat"));
    if (spoken) {
        QFont small = plainFont(painter);
        small.setPixelSize(12);
        painter->setFont(small);
        painter->setPen(blendColours(scheme.feltText, scheme.felt, 0.25));
        const QString by = m_game->seat(m_game->bidder()).human()
                               ? tr("annoncé par vous — ×%1").arg(contractMultiplier(m_game->bid()))
                               : tr("annoncé par %1 — ×%2")
                                     .arg(m_game->seat(m_game->bidder()).name)
                                     .arg(contractMultiplier(m_game->bid()));
        painter->drawText(QRectF(m_cross.left(), badge.bottom() + 4.0, m_cross.width(), 16.0),
                          Qt::AlignCenter, by);
    }

    // Ce que chacun a dit, à sa place autour de la table. L'enchère est une
    // conversation, et c'est la seule façon de la montrer.
    QFont said = plainFont(painter);
    said.setBold(true);
    said.setPixelSize(13);
    const QFontMetricsF saidMetrics(said);
    painter->setFont(said);

    const QPointF middle = m_cross.center();
    const qreal aside = heart.width() / 2.0;
    // Assez haut pour passer au-dessus de la légende qui coiffe la retourne ou
    // le contrat, et au-dessous de celle qui dit qui l'a annoncé : sinon la
    // pastille du partenaire vient se poser sur le mot « le contrat ».
    const qreal above = heart.height() / 2.0 + 44.0;

    for (int at = 0; at < m_game->seatCount(); ++at) {
        const Seat &seat = m_game->seat(at);
        const bool speaking = m_game->turn() == at;
        if (seat.said.isEmpty() && !speaking) continue;

        const QString shown =
            speaking ? (seat.human() ? tr("à vous de parler") : tr("à lui de parler"))
                     : seat.said;
        QRectF chip(0, 0, saidMetrics.horizontalAdvance(shown) + 18.0,
                    saidMetrics.height() + 6.0);
        // La pastille de chaque parole est à la place de celui qui la dit,
        // sur le même cercle que les places du pli : la croix à 4 joueurs,
        // un cercle à 3 ou 5. Sans cela, à 5 joueurs deux pastilles se
        // poseraient l'une sur l'autre et l'on ne verrait que trois joueurs.
        const int seats = m_game->seatCount();
        const int at_ = compassOf(at);
        const qreal angle = kPi / 2.0 + at_ * 2.0 * kPi / seats;
        chip.moveCenter(middle + QPointF((aside + chip.width() / 2.0 + 16.0) * std::cos(angle),
                                         above * std::sin(angle)));
        painter->setPen(Qt::NoPen);
        painter->setBrush(speaking ? scheme.accent
                                   : blendColours(scheme.feltEdge, scheme.surface, 0.30));
        painter->drawRoundedRect(chip, chip.height() / 2.0, chip.height() / 2.0);
        painter->setPen(speaking ? scheme.accentText : scheme.feltText);
        painter->drawText(capCentred(chip, saidMetrics), Qt::AlignCenter, shown);
    }
}

void TableView::paintChien(QPainter *painter) {
    const Palette &scheme = currentPalette();
    if (m_chienBoxes.isEmpty()) return;

    const bool open = m_game->phase() == Phase::Chien;
    const QVector<Card> &chien = m_game->chien();

    painter->setFont(capsFont(painter, 11));
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_cross.left(), m_chienBoxes.first().top() - 22.0, m_cross.width(),
                             16.0),
                      Qt::AlignCenter, tr("Le chien"));

    for (int i = 0; i < m_chienBoxes.size(); ++i) {
        const QRectF box = m_chienBoxes.at(i);
        if (open && i < chien.size()) {
            if (flying(chien.at(i).id)) continue;
            drawCardFace(painter, box, chien.at(i), scheme);
        } else if (!open) {
            // L'écart en cours : la place du chien reste marquée, vide, le
            // temps que le preneur choisisse ses six cartes. Sans cela le
            // milieu du drap se vide d'un coup et la table paraît finie.
            drawCardSlot(painter, box, scheme, QString());
        }
    }

    QFont small = plainFont(painter);
    small.setPixelSize(13);
    painter->setFont(small);
    painter->setPen(blendColours(scheme.feltText, scheme.felt, 0.22));

    const int taker = m_game->taker();
    const QString who = taker < 0 || m_game->seat(taker).human()
                            ? tr("vous")
                            : m_game->seat(taker).name;
    const QString line =
        open ? tr("%1 points dans le chien — il revient à %2")
                   .arg(pointsText(totalPoints(chien)), who)
             : (taker >= 0 && m_game->seat(taker).human()
                    ? tr("choisissez six cartes à mettre de côté")
                    : tr("%1 fait son écart").arg(who));
    painter->drawText(QRectF(m_cross.left(), m_chienBoxes.first().bottom() + 8.0,
                             m_cross.width(), 18.0),
                      Qt::AlignCenter, line);
}

void TableView::paintTrick(QPainter *painter) {
    const Palette &scheme = currentPalette();
    const int master = m_game->trickMaster();

    // Les quatre places, vides ou non. Une place vide porte le nom de celui
    // qui y posera sa carte : c'est ce qui fait de la croix une table plutôt
    // que quatre rectangles.
    for (int at = 0; at < m_game->seatCount(); ++at) {
        const QRectF slot = m_slotBoxes[compassOf(at)];
        if (slot.isNull()) continue;
        bool filled = false;
        for (const Played &played : m_game->trick()) {
            if (played.seat == at) filled = true;
        }
        if (filled) continue;
        drawCardSlot(painter, slot, scheme,
                     m_game->seat(at).human() ? tr("vous") : m_game->seat(at).name);
    }

    for (const Played &played : m_game->trick()) {
        const QRectF slot = m_slotBoxes[compassOf(played.seat)];
        if (slot.isNull() || flying(played.card.id)) continue;
        drawCardFace(painter, slot, played.card, scheme);

        if (played.seat != master) continue;
        // Qui tient le pli, dit par un liseré doré : sans ça, un pli de
        // quatre cartes dont l'une est un atout est une devinette.
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(scheme.gold, 2.4));
        painter->drawRoundedRect(slot.adjusted(-3, -3, 3, 3),
                                 std::max(3.0, slot.width() * 0.085),
                                 std::max(3.0, slot.width() * 0.085));
    }
}

void TableView::paintSide(QPainter *painter) {
    const Palette &scheme = currentPalette();
    if (m_side.width() < 90) return;

    const QRectF panel = m_side;
    QPainterPath shape;
    shape.addRoundedRect(panel, 12, 12);
    QColor bed = scheme.feltEdge;
    bed.setAlpha(130);
    painter->setPen(Qt::NoPen);
    painter->fillPath(shape, bed);
    painter->setBrush(Qt::NoBrush);
    QColor braid = scheme.trim;
    braid.setAlpha(120);
    painter->setPen(QPen(braid, 1.2));
    painter->drawPath(shape);

    const QRectF inner = panel.adjusted(12, 12, -12, -12);
    qreal y = inner.top();

    const int me = mySeat();
    const int taker = m_game->taker();
    const bool known = taker >= 0;

    // Le contrat, en gros : c'est le fait le plus important de la donne, et il
    // doit se lire d'un coup d'œil depuis l'autre bout de la pièce. L'atout
    // n'est pas une information au tarot -- il est imprimé sur les cartes.
    painter->setFont(capsFont(painter, 10));
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(inner.left(), y, inner.width(), 14.0),
                      Qt::AlignLeft | Qt::AlignVCenter, tr("Le contrat"));
    y += 17.0;

    QFont small = plainFont(painter);
    small.setPixelSize(11);

    if (known) {
        QFont name = plainFont(painter);
        name.setBold(true);
        name.setPixelSize(16);
        painter->setFont(name);
        const QFontMetricsF nameMetrics(name);
        painter->setPen(scheme.gold);
        painter->drawText(QRectF(inner.left(), y, inner.width(), nameMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, contractName(m_game->contract()));
        y += nameMetrics.height() + 2.0;

        painter->setFont(small);
        painter->setPen(blendColours(scheme.feltText, scheme.felt, 0.25));
        painter->drawText(QRectF(inner.left(), y, inner.width(), 15.0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          m_game->seat(taker).human()
                              ? tr("pris par vous, ×%1").arg(m_game->multiplier())
                              : tr("pris par %1, ×%2")
                                    .arg(m_game->seat(taker).name)
                                    .arg(m_game->multiplier()));
        y += 19.0;
    } else {
        QFont wait = plainFont(painter);
        wait.setPixelSize(12);
        painter->setFont(wait);
        painter->setPen(blendColours(scheme.feltText, scheme.felt, 0.25));
        painter->drawText(QRectF(inner.left(), y, inner.width(), 30.0),
                          Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                          tr("pas encore : on enchérit"));
        y += 34.0;
    }

    // Les bouts tombés dans le tas du preneur, et ce qu'ils lui demandent. Le
    // contrat du tarot n'est pas un nombre fixé mais un nombre qui baisse à
    // mesure qu'il ramasse des bouts, et c'est ce que ces trois chiffres-là
    // disent.
    if (known && m_game->phase() != Phase::Chien && m_game->phase() != Phase::Ecart) {
        painter->setFont(capsFont(painter, 10));
        painter->setPen(scheme.feltText);
        painter->drawText(QRectF(inner.left(), y, inner.width(), 14.0),
                          Qt::AlignLeft | Qt::AlignVCenter, tr("Les bouts"));
        y += 16.0;

        QFont figures = plainFont(painter);
        figures.setBold(true);
        figures.setPixelSize(14);
        painter->setFont(figures);
        const QFontMetricsF figureMetrics(figures);
        painter->setPen(scheme.gold);
        painter->drawText(QRectF(inner.left(), y, inner.width(), figureMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          countedText(m_game->takerBouts(), tr("bout tombé"),
                                      tr("bouts tombés")));
        painter->setFont(small);
        painter->setPen(blendColours(scheme.feltText, scheme.felt, 0.25));
        painter->drawText(QRectF(inner.left(), y, inner.width(), figureMetrics.height()),
                          Qt::AlignRight | Qt::AlignVCenter,
                          tr("but %1").arg(pointsText(m_game->takerTarget())));
        y += figureMetrics.height() + 8.0;
    }

    // La marque. Deux camps, deux lignes : ce que chacun a ramassé dans cette
    // donne, et combien de plis il a pris.
    painter->setFont(capsFont(painter, 10));
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(inner.left(), y, inner.width(), 14.0),
                      Qt::AlignLeft | Qt::AlignVCenter, tr("Les plis"));
    y += 16.0;
    painter->setPen(QPen(blendColours(scheme.feltText, scheme.felt, 0.55), 1.0));
    painter->drawLine(QPointF(inner.left(), y), QPointF(inner.right(), y));
    y += 5.0;

    const int myTeam = known ? m_game->teamOf(me) : kDefenceTeam;
    for (int step = 0; step < kTeamCount; ++step) {
        const int team = step == 0 ? myTeam : Tarot::otherTeam(myTeam);
        const bool ours = team == myTeam;
        const int hue = team == kTakerTeam && known && taker != me
                            ? m_game->seat(taker).hue
                            : currentPalette().hue;
        const QColor colour = rivalColour(hue);

        QFont name = plainFont(painter);
        name.setBold(ours);
        name.setPixelSize(12);
        painter->setFont(name);
        const QFontMetricsF nameMetrics(name);
        painter->setPen(Qt::NoPen);
        painter->setBrush(colour);
        painter->drawEllipse(QRectF(inner.left(), y + nameMetrics.height() / 2.0 - 3.5, 7.0, 7.0));
        painter->setPen(scheme.feltText);
        QString label;
        if (!known) {
            label = ours ? tr("Vous") : tr("Les autres");
        } else if (team == kTakerTeam) {
            label = taker == me ? tr("Vous, preneur") : tr("%1, preneur").arg(
                                      m_game->seat(taker).name);
        } else {
            label = taker == me
                        ? tr("Les %1")
                              .arg(countedText(m_game->seatCount() - 1, tr("autre"),
                                               tr("autres")))
                        : tr("La défense");
        }
        painter->drawText(
            QRectF(inner.left() + 12.0, y, inner.width() - 12.0, nameMetrics.height()),
            Qt::AlignLeft | Qt::AlignVCenter,
            nameMetrics.elidedText(label, Qt::ElideRight, inner.width() - 12.0));
        y += nameMetrics.height();

        QFont figures = plainFont(painter);
        figures.setBold(true);
        figures.setPixelSize(15);
        painter->setFont(figures);
        const QFontMetricsF figureMetrics(figures);
        painter->setPen(colour);
        painter->drawText(
            QRectF(inner.left() + 12.0, y, inner.width() - 12.0, figureMetrics.height()),
            Qt::AlignLeft | Qt::AlignVCenter, tr("%1 pts").arg(pointsText(m_game->teamPoints(team))));
        painter->setFont(small);
        painter->setPen(blendColours(scheme.feltText, scheme.felt, 0.25));
        painter->drawText(
            QRectF(inner.left() + 12.0, y, inner.width() - 12.0, figureMetrics.height()),
            Qt::AlignRight | Qt::AlignVCenter,
            countedText(m_game->teamTricks(team), tr("pli"), tr("plis")));
        y += figureMetrics.height() + 7.0;
    }

    // Votre total depuis le début de la partie, et où en est la partie. Les
    // quatre totaux ne tiendraient pas dans la colonne ; la feuille de marque
    // les donne tous les quatre à la fin de chaque donne.
    painter->setFont(small);
    painter->setPen(blendColours(scheme.feltText, scheme.felt, 0.30));
    const int mine = me >= 0 ? m_game->seatScore(me) : 0;
    painter->drawText(QRectF(inner.left(), y, inner.width(), 15.0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      tr("votre total : %1%2")
                          .arg(mine > 0 ? QStringLiteral("+") : QString(),
                               pointsText(mine)));
    y += 16.0;
    painter->drawText(QRectF(inner.left(), y, inner.width(), 15.0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      tr("donne %1 sur %2")
                          .arg(m_game->donneNumber() + 1)
                          .arg(m_game->targetDonnes()));
    y += 20.0;

    // Ce qu'il faut faire, pour que les chiffres au-dessus veuillent dire
    // quelque chose.
    if (!known || y > inner.bottom() - 16.0) return;
    painter->setFont(small);
    painter->setPen(blendColours(scheme.feltText, scheme.felt, 0.30));
    const int left = m_game->takerTarget() - m_game->teamPoints(kTakerTeam);
    const bool ourContract = taker == me;
    QString need;
    if (left > 0) {
        need = ourContract ? tr("il vous manque %1 points").arg(pointsText(left))
                           : tr("il lui manque %1 points").arg(pointsText(left));
    } else {
        need = ourContract ? tr("le contrat est fait") : tr("son contrat est fait");
    }
    painter->drawText(QRectF(inner.left(), y, inner.width(), inner.bottom() - y),
                      Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, need);
}

void TableView::paintRail(QPainter *painter) {
    const Palette &scheme = currentPalette();
    qreal textLeft = m_rail.left() + 6.0;

    // Ce que le pli vaut déjà. C'est le chiffre qui décide si on prend ou si
    // on laisse, et personne ne le compte de tête à la vitesse où ça va.
    if (m_game->phase() == Phase::Playing || m_game->phase() == Phase::TrickOver) {
        const int pot = m_game->trickValue();
        QFont tally = plainFont(painter);
        tally.setBold(true);
        tally.setPixelSize(14);
        painter->setFont(tally);
        const QString text =
            pot > 0 ? tr("le pli : %1 pts").arg(pointsText(pot)) : tr("le pli : rien");
        const QFontMetricsF metrics(tally);
        QRectF badge(m_rail.left() + 6.0, 0, metrics.horizontalAdvance(text) + 20.0, 24.0);
        badge.moveCenter(QPointF(badge.center().x(), m_rail.center().y()));
        painter->setPen(Qt::NoPen);
        painter->setBrush(blendColours(scheme.feltEdge, scheme.surface, 0.26));
        painter->drawRoundedRect(badge, 12, 12);
        painter->setPen(scheme.feltText);
        painter->drawText(capCentred(badge, metrics), Qt::AlignCenter, text);
        textLeft = badge.right() + 14.0;
    }

    if (m_line.isEmpty()) return;
    QFont line = plainFont(painter);
    line.setPixelSize(14);
    line.setBold(m_trouble);
    painter->setFont(line);
    painter->setPen(m_trouble ? blendColours(scheme.danger, QColor(Qt::white), 0.35)
                              : scheme.feltText);
    painter->drawText(QRectF(textLeft, m_rail.top(),
                             std::max(60.0, m_rail.right() - textLeft - 6.0), m_rail.height()),
                      Qt::AlignRight | Qt::AlignVCenter | Qt::TextWordWrap, m_line);
}

void TableView::paintHand(QPainter *painter) {
    const Palette &scheme = currentPalette();
    const int me = mySeat();
    if (me < 0) return;
    const Seat &mine = m_game->seat(me);

    // Ce qu'on a le droit de jouer. Les autres cartes sont dessinées éteintes
    // plutôt que refusées après coup : fournir à la couleur, couper, monter --
    // trois règles qui s'apprennent en voyant douze cartes sur dix-huit
    // s'éteindre, et qui ne s'apprennent pas en lisant un refus en rouge.
    //
    // L'écart suit la même règle : un roi et un bout ne s'écartent pas, et ils
    // sont éteints pendant qu'on choisit les six cartes à mettre de côté.
    QVector<int> playable;
    const bool choosing = m_game->phase() == Phase::Playing && m_game->turn() == me;
    const bool discarding = m_game->phase() == Phase::Ecart && m_game->taker() == me;
    if (choosing) playable = m_game->legalCards(me);

    for (const Spot &spot : m_handBoxes) {
        const Card *card = findCard(mine.hand, spot.id);
        if (!card) continue;
        // Une carte encore en l'air est rangée en main avant que son vol
        // commence : sans cette question-là elle se trouverait dessinée deux
        // fois, une fois arrivée et une fois en chemin.
        if (flying(spot.id)) continue;
        CardLook look;
        look.chosen = m_chosen == spot.id || m_picked.contains(spot.id);
        look.lit = m_hovered == spot.id;
        look.dimmed = (choosing && !playable.contains(spot.id)) ||
                      (discarding && !m_game->mayDiscard(*card) && !m_picked.contains(spot.id));
        QRectF box = handBoxNow(spot.id, spot.box);
        if (look.chosen) {
            box.translate(0, -kLift);
        } else if (look.lit && !look.dimmed) {
            box.translate(0, -6.0);
        }
        drawCardFace(painter, box, *card, scheme, look);
    }

    if (mine.hand.isEmpty()) {
        QFont empty = plainFont(painter);
        empty.setPixelSize(15);
        painter->setFont(empty);
        painter->setPen(scheme.feltText);
        painter->drawText(m_handRow, Qt::AlignCenter, tr("Plus une carte en main."));
        return;
    }

    // Et le tour de piste quand c'est à vous : la main entière s'éclaire, ce
    // qui est la façon la plus courte de dire « on vous attend ».
    if (!myTurn() && !discarding) return;
    QColor ring = scheme.gold;
    ring.setAlpha(150);
    painter->setPen(QPen(ring, 2.0, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(m_handRow.adjusted(-4, -4, 4, 4), 12, 12);
}

void TableView::paintFlights(QPainter *painter) {
    const Palette &scheme = currentPalette();
    for (const Flight &flight : m_flights) {
        // Elle attend son tour dans la distribution : elle est encore dans le
        // talon, et n'a rien à montrer en chemin.
        if (flight.along < 0.0) continue;
        const qreal along = plateau(flight.along);
        const QPointF at = flight.from + (flight.to - flight.from) * along;
        // Le talon, le pli et la main ne sont pas à la même échelle, alors une
        // carte qui va de l'un à l'autre grandit en chemin.
        QSizeF size = flight.size;
        if (flight.grow.isValid()) {
            size = QSizeF(flight.size.width() + (flight.grow.width() - flight.size.width()) * along,
                          flight.size.height() +
                              (flight.grow.height() - flight.size.height()) * along);
        }
        const QRectF box(at, size);
        painter->save();
        // Une carte lancée tourne un peu, et se redresse en arrivant.
        painter->translate(box.center());
        painter->rotate((1.0 - along) * 14.0);

        if (flight.flips) {
            // Le demi-tour. La largeur apparente est le cosinus de l'angle, ce
            // qui est exactement ce que fait une carte qu'on retourne entre
            // deux doigts : pleine de dos, nulle de profil, pleine de face.
            // Jamais tout à fait nulle, pour qu'il reste la tranche à voir au
            // moment où elle passe.
            const qreal turned = std::cos(along * M_PI);
            painter->scale(std::max(0.03, std::abs(turned)), 1.0);
            painter->translate(-box.center());
            if (turned > 0.0) {
                drawCardBack(painter, box, scheme);
            } else {
                drawCardFace(painter, box, flight.card, scheme);
            }
            painter->restore();
            continue;
        }

        painter->translate(-box.center());
        if (flight.faceUp) {
            drawCardFace(painter, box, flight.card, scheme);
        } else {
            drawCardBack(painter, box, scheme);
        }
        painter->restore();
    }
}

// ---------------------------------------------------------------------------
// La souris.

int TableView::cardAt(const QPointF &where) const {
    // À l'envers : celle qui est dessinée en dernier est celle du dessus, et
    // celle du dessus est celle qu'on voulait.
    for (int i = m_handBoxes.size() - 1; i >= 0; --i) {
        const int id = m_handBoxes.at(i).id;
        // Une carte qui glisse, ou qui est soulevée, se clique là où elle est
        // et non là où elle sera.
        QRectF box = handBoxNow(id, m_handBoxes.at(i).box);
        if (m_chosen == id || m_picked.contains(id)) box.translate(0, -kLift);
        if (box.contains(where)) return id;
    }
    return -1;
}

int TableView::seatAt(const QPointF &where) const {
    for (const Spot &spot : m_seatBoxes) {
        if (spot.box.contains(where)) return spot.id;
    }
    return -1;
}

void TableView::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        clearChosen();
        return;
    }
    const int card = cardAt(event->position());
    if (card >= 0) {
        Q_EMIT cardPressed(card);
        return;
    }
    clearChosen();
}

void TableView::mouseDoubleClickEvent(QMouseEvent *event) {
    const int card = cardAt(event->position());
    if (card < 0) return;
    Q_EMIT cardDoubleClicked(card);
}

void TableView::mouseMoveEvent(QMouseEvent *event) {
    const QPointF where = event->position();
    const int card = cardAt(where);
    const int seat = seatAt(where);
    if (card == m_hovered && seat == m_hoveredSeat) return;
    if (seat != m_hoveredSeat) {
        // Changement de plaque survolée, ou sortie de la plaque courante :
        // l'aperçu est libéré -- il se referme si le pointeur l'a quitté, il
        // subsiste si le pointeur est passé sur lui -- et la minuterie est
        // relancée pour la plaque courante.
        m_hoveredSeat = seat;
        m_glanceBeat->stop();
        releaseRivalGlance(this);
        if (seat >= 0) m_glanceBeat->start();
    }
    m_hovered = card;
    setCursor(card >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

void TableView::wheelEvent(QWheelEvent *event) {
    // Ctrl et la molette, comme partout ailleurs. La molette seule est laissée
    // tranquille : on la pousse sans y penser en jouant, et un drap qui change
    // de taille tout seul serait une mauvaise surprise.
    if (!event->modifiers().testFlag(Qt::ControlModifier)) {
        QWidget::wheelEvent(event);
        return;
    }
    const int turn = event->angleDelta().y();
    if (turn != 0) setZoom(m_zoom + (turn > 0 ? kZoomStep : -kZoomStep));
    event->accept();
}

void TableView::leaveEvent(QEvent *) {
    m_hovered = -1;
    m_hoveredSeat = -1;
    m_glanceBeat->stop();
    releaseRivalGlance(this);
    update();
}

void TableView::hideEvent(QHideEvent *event) {
    // Retour au menu ou fermeture de la fenêtre : l'aperçu doit être fermé,
    // faute de quoi il subsisterait à l'écran sans la table à laquelle il se
    // rapporte.
    m_hoveredSeat = -1;
    m_glanceBeat->stop();
    hideRivalGlance(this);
    QWidget::hideEvent(event);
}
