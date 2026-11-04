#pragma once

#include "Cards.h"

#include <QPixmap>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class RivalCard;
class TileButton;

// Le menu d'ouverture : les six joueurs, et les cinq tables.
//
// Il n'y a ni bouton « nouvelle partie » ni boîte de dialogue à traverser :
// presser une table est ce qui lance la partie, parce que choisir jusqu'où
// elle va est le seul choix qu'une nouvelle partie demande. Tout ce qui se
// choisit est donc au-dessus d'elles -- la question d'avant, qui au tarot
// n'est pas « contre qui » mais « avec qui » : le premier invité s'assoit en
// face de vous -- et les tables ferment la page, parce qu'une fois pressées
// il n'y a plus de page.
//
// Le profil n'est plus ici : il se tient dans la bande du haut, où il reste
// visible pendant la partie. Et ce qui se fait d'un joueur se fait sur lui,
// d'un clic droit, plutôt que dans les réglages.
//
// Sur le tapis, et non sur une page à part : c'est le même drap que l'écran
// titre et que la partie, et le logo descend de l'un pour se poser en tête de
// celui-ci. Ce qui est posé dessus est donc sombre et bordé d'or -- ce qui se
// lit sur un drap vert sombre est clair, pas pâle.
class TablePicker : public QWidget {
    Q_OBJECT

public:
    explicit TablePicker(QWidget *parent = nullptr);

    // Qui a été invité, dans l'ordre. Le premier s'assoit en face de vous ;
    // les places qui restent vont aux moins joués.
    QVector<QString> invited() const;
    // Combien de joueurs à la table : 3, 4 ou 5.
    int seats() const { return m_seats; }
    void setSeats(int seats);

    // Après un changement de profil, ou quand l'expérience a bougé.
    void reread();
    void refresh();

    // Où le logo se pose, dans les coordonnées de la page : c'est là que le
    // vol depuis l'écran titre se termine.
    QRectF markRect() const;
    // Le temps du vol, c'est le calque qui porte le logo.
    void setMarkHidden(bool hidden);
    // La page paraît : ce qui est posé sur le drap monte de rien à tout
    // pendant que le logo descend. Sans cela le logo volerait au-dessus d'un
    // menu déjà entièrement là, et le vol n'aurait l'air de rien.
    void beginEntrance();

Q_SIGNALS:
    void tableChosen(const QString &id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildFelt();

    void showRivalMenu(RivalCard *card, const QPoint &where);
    void renameRival(const QString &rivalId);
    void askResetRival(const QString &rivalId);

    // Le tapis, peint une fois : la page défile, et refaire le tissage à
    // chaque cran de molette serait du travail jeté.
    QPixmap m_felt;
    // La bande du haut, qui ne défile pas : le logo s'y tient.
    QWidget *m_header = nullptr;
    // Tout ce qui n'est pas la bande du logo : c'est lui qui paraît.
    QWidget *m_content = nullptr;
    QLabel *m_rivalNote = nullptr;
    QLabel *m_footer = nullptr;
    QVector<QWidget *> m_headings;
    QVector<TileButton *> m_tiles;
    QVector<TileButton *> m_seatTiles;
    QVector<RivalCard *> m_cards;
    int m_seats = kSeatCount;
    // L'ordre dans lequel on les a invités, parce que le premier ne s'assoit
    // pas à la même place que le deuxième.
    QVector<QString> m_order;
    bool m_markHidden = false;
};
