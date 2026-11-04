#pragma once

#include "Cards.h"

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QVector>
#include <QWidget>

class QTimer;
class QWheelEvent;
class Tarot;

// La table, à l'écran.
//
// Un seul widget pour tout le drap : les plaques des trois autres autour du
// pli, le pli en croix au milieu, la marque dans la colonne de droite, et vos
// dix-huit cartes en bas. C'est un seul widget et pas six parce que le drap est
// une seule surface -- les quatre cartes d'un pli doivent pouvoir voler
// jusqu'à celui qui l'a ramassé, et un assemblage de panneaux est exactement
// la chose qui ne le permet pas.
//
// Les plaques se placent de deux façons selon la place qu'il y a. Quand la
// fenêtre est large, chacun est dessiné là où il est assis : le partenaire en
// face, les deux adversaires à gauche et à droite, et la carte que chacun joue
// tombe du côté de sa plaque. Quand elle ne l'est pas, les trois se rangent en
// bande au-dessus du tapis -- c'est moins parlant, mais trois plaques serrées
// entre deux bords sont illisibles, et une plaque illisible ne dit rien du
// tout.
//
// Il peint la partie et il rapporte les clics. Il lit les règles pour savoir
// quelles cartes sont jouables -- une carte interdite est dessinée éteinte,
// ce qui est la seule façon d'enseigner l'obligation de fournir sans écrire un
// paragraphe -- mais il ne décide rien : ce qui arrive quand on presse une
// carte appartient à la page au-dessus.
class TableView : public QWidget {
    Q_OBJECT

public:
    explicit TableView(QWidget *parent = nullptr);

    // La partie montrée. Pas possédée ; la vue la lit et ne la change jamais.
    void watch(const Tarot *game);

    // La carte choisie dans la main. Une seule, parce qu'on ne joue qu'une
    // carte à la fois : c'est une affaire d'écran, et c'est la page qui décide
    // ce qu'un clic en fait.
    int chosen() const { return m_chosen; }
    void setChosen(int cardId);
    void clearChosen();

    // L'écart, lui, se choisit à six cartes à la fois : c'est le seul moment
    // du jeu où l'on désigne plusieurs cartes avant de les poser. Les cartes
    // retenues sont dessinées soulevées, comme une carte choisie, et la page
    // décide quand les six partent.
    const QSet<int> &picked() const { return m_picked; }
    void togglePicked(int cardId);
    void clearPicked();

    // Une ligne de ce qui se passe, écrite sur le drap au-dessus de la main.
    void say(const QString &line);
    void sayTrouble(const QString &line);

    // Ce qui vient d'arriver, pour qu'on le voie arriver : une carte qui vole
    // jusqu'à sa place dans le pli, et les quatre cartes du pli qui s'en vont
    // chez celui qui l'a pris.
    void notePlayed(int seat, const Card &card);
    void noteTrickTaken(int winner);
    // La donne qui se distribue : les cartes partent du milieu du drap et
    // vont chez chacun, celles qui sont à vous en se retournant en chemin.
    void noteDealt();
    // Le chien qui entre en main : les six cartes partent de leur place au
    // milieu du drap, où tout le monde vient de les voir, et vont se ranger
    // dans la main du preneur. Elles ne se retournent pas en chemin, ayant été
    // montrées.
    void noteChienTaken(const QVector<int> &cardIds);
    // Et l'écart qui en sort : les six cartes mises de côté quittent la main
    // vers le coin du tapis où le preneur pose son tas.
    void noteEcart(const QVector<int> &cardIds);
    void stopAnimations();

    // Pose les couleurs du moment sur le drap, pour le bouton sombre.
    void refresh();

    // Où les cartes de la donne se trouvent à l'écran à la fin : ramassées au
    // milieu, en piles, là où elles sont vraiment. C'est ce qu'il faut au feu
    // d'artifice pour que les cartes aient l'air de se soulever du tapis
    // plutôt que d'arriver de nulle part.
    void cardsOnScreen(QVector<Card> *cards, QVector<QPointF> *at) const;
    qreal trickCardHeight() const { return m_trickHeight; }

    // La taille des cartes, en pour cent.
    //
    // Un jeu installé apporte des images d'une finesse donnée -- les
    // CARDS.DLL de Windows font 71 sur 96 -- et une figure réduite au quart de
    // cela sur le tapis n'est plus qu'un bruit de couleurs. Aucun filtre ne
    // rend des pixels qui n'existent pas : ce qu'il faut, c'est pouvoir les
    // dessiner plus grandes. Le zoom tire sur toutes les tailles de carte à la
    // fois, la main, le pli et la retourne, et chaque bande du drap grandit
    // avec elles plutôt que de les laisser déborder.
    void setZoom(int percent);
    int zoom() const { return m_zoom; }

Q_SIGNALS:
    void cardPressed(int cardId);
    void cardDoubleClicked(int cardId);
    // Changé à la molette, pour que la fenêtre le garde.
    void zoomChanged(int percent);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    struct Spot {
        int id = -1;
        QRectF box;
    };

    struct Flight {
        Card card;
        bool faceUp = true;
        // Le demi-tour : la carte part de dos, passe de profil à mi-chemin et
        // arrive face en haut. Réservé aux cartes qui vous arrivent -- c'est
        // le seul moment du jeu où une carte se retourne pour vous.
        bool flips = false;
        QPointF from;
        QPointF to;
        QSizeF size;
        // Où elle va, quand ce n'est pas la taille d'où elle part.
        QSizeF grow;
        // Négative, la carte n'a pas encore quitté sa place : elle attend son
        // tour dans la distribution. Voir paintFlights().
        qreal along = 0.0;
        qreal speed = 0.055;
    };

    void relayout();
    void layoutSeats();
    // Place-t-on les plaques autour du tapis, ou en bande au-dessus ? Décidé à
    // la largeur qu'il reste au pli une fois les deux plaques de côté posées :
    // en dessous d'une croix de cartes, ça ne tient pas.
    bool roomAround(qreal plaque) const;
    void layoutSlots();
    void layoutHand();

    void paintSeats(QPainter *painter);
    void paintSeatPlaque(QPainter *painter, const QRectF &box, int at);
    void paintTapis(QPainter *painter);
    void paintBidding(QPainter *painter);
    void paintChien(QPainter *painter);
    void paintTrick(QPainter *painter);
    void paintSide(QPainter *painter);
    void paintRail(QPainter *painter);
    void paintHand(QPainter *painter);
    void paintFlights(QPainter *painter);

    int cardAt(const QPointF &where) const;
    // La place dont la plaque est sous le pointeur, ou -1 : c'est ce qui
    // déclenche l'aperçu de l'adversaire qui y est assis.
    int seatAt(const QPointF &where) const;
    // Une carte encore en l'air n'est pas dessinée à sa place : elle y serait
    // deux fois, une fois posée et une fois en vol, et on verrait double.
    bool flying(int cardId) const;
    int mySeat() const;
    // Où la place `at` se trouve autour du drap, vue depuis la vôtre : zéro en
    // bas, puis à gauche, en face, à droite.
    int compassOf(int at) const;
    QPointF seatAnchor(int at) const;
    bool myTurn() const;

    void tick();
    void wake();

    // La place qu'occupe la i-ème carte d'une main qui en compte `count`.
    //
    // Fonction pure de ces deux nombres : c'est ce qui permet de savoir où les
    // cartes étaient avant la prise sans l'avoir noté -- la main d'avant est
    // la main d'après moins trois cartes.
    QRectF handSlot(int index, int count) const;

    // Où une carte de la main est *à cet instant* : sa place, ou le point du
    // trajet où elle en est si la main est en train de s'écarter. Le dessin et
    // le pointage s'en servent tous les deux -- c'est ce qui garantit qu'on
    // clique la carte qu'on voit.
    QRectF handBoxNow(int id, const QRectF &box) const;

    // Le milieu du drap, d'où les cartes sont données.
    QRectF dealBox() const;

    const Tarot *m_game = nullptr;

    QRectF m_seatsRow;
    QRectF m_tapis;
    // Ce qui reste du tapis au pli lui-même : tout le tapis quand les plaques
    // sont en bande au-dessus, et le milieu quand elles sont sur les côtés.
    QRectF m_cross;
    QRectF m_side;
    QRectF m_rail;
    QRectF m_handRow;

    bool m_aroundTable = false;
    QVector<Spot> m_seatBoxes;      // les autres, autour ou en bande
    QVector<QRectF> m_slotBoxes;    // les places du pli, en cercle (en croix à 4)
    // Les six cartes du chien, posées en rang au milieu du drap le temps que
    // la table les lise.
    QVector<QRectF> m_chienBoxes;
    QVector<Spot> m_handBoxes;
    qreal m_trickHeight = 92.0;

    int m_chosen = -1;
    QSet<int> m_picked;
    int m_hovered = -1;
    int m_hoveredSeat = -1;

    QString m_line;
    bool m_trouble = false;

    QTimer *m_ticker = nullptr;
    // Le maintien du pointeur sur une plaque avant que l'aperçu paraisse : un
    // déplacement à travers la table n'en déclenche aucun.
    QTimer *m_glanceBeat = nullptr;
    QVector<Flight> m_flights;

    // Où chaque carte de la main a été posée la dernière fois qu'on a disposé
    // le drap. La disposition est refaite pendant que la carte quitte la main,
    // si bien qu'au moment d'animer son départ sa place n'est déjà plus dans
    // m_handBoxes : c'est ici qu'on la retrouve.
    QHash<int, QRectF> m_handWas;

    // L'écartement de la main pendant que les cartes de la prise volent vers
    // leurs places : d'où chacune part, et où en est le mouvement.
    QHash<int, QRectF> m_slideFrom;
    qreal m_slideAlong = 1.0;
    QRectF m_handRowWas;

    int m_zoom = 100;
};
