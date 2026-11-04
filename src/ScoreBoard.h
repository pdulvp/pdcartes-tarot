#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QPushButton;
class Tarot;

// La feuille de marque, posée sur le tapis à la fin d'une donne.
//
// Elle couvre le drap au lieu de le remplacer : les plaques et le dernier pli
// sont encore là dessous, ce qui est ce qu'on veut regarder pendant qu'on lit
// ce que la donne a coûté.
//
// Deux formes, la même carte : à la fin d'une donne c'est ce que celle-ci a
// donné et où en sont les quatre joueurs ; à la fin de la partie elle dit qui
// l'a gagnée.
//
// Quatre lignes et non deux camps, parce qu'au tarot la marque est
// individuelle : le preneur prend ou paie trois fois ce que chaque défenseur
// paie ou prend, et la somme des quatre lignes est nulle. C'est ce qui permet
// de vérifier la feuille d'un coup d'œil.
class ScoreBoard : public QWidget {
    Q_OBJECT

public:
    explicit ScoreBoard(QWidget *parent = nullptr);

    void showDonne(const Tarot *game);
    void showMatch(const Tarot *game);
    void hideBoard();

    void refresh();

Q_SIGNALS:
    // La donne suivante, ou une nouvelle partie : selon ce que la feuille
    // montre.
    void goOn();
    void leave();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Line {
        QString name;
        int hue = 0;
        bool taker = false;
        bool callee = false;   // à 5 joueurs, le partenaire appelé
        int tricks = 0;      // les plis de son camp
        int points = 0;      // ce que son camp a ramassé, en demi-points
        int gained = 0;      // ce qu'il marque, signé
        int total = 0;
        bool ours = false;
        bool winner = false;
    };

    QSizeF measure() const;
    void placeButtons();
    void dressButtons();

    QVector<Line> m_lines;
    QString m_headline;
    QString m_aside;
    QStringList m_notes;
    bool m_matchOver = false;
    bool m_youWon = false;

    QPushButton *m_onwards = nullptr;
    QPushButton *m_away = nullptr;
};
