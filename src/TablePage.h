#pragma once

#include "Brain.h"
#include "Tables.h"
#include "Tarot.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class ScoreBoard;
class TableView;
class VictoryEffect;

// Tout ce qu'on peut régler, et ce n'est pas grand-chose : un tarot demande
// une table, et deux ou trois commodités.
struct GameSettings {
    QString tableId;
    // Le nombre de joueurs à la table : 3, 4 ou 5 (règlement FFT). Choisi sur
    // la page de menu, et gardé d'une séance à l'autre.
    int seats = kSeatCount;
    // Les deux annonces qui s'ajoutent à la marque sans entrer dans le
    // contrat. Ce sont des règles et non des commodités : elles ne changent
    // pas une partie en cours.
    bool poignees = true;
    bool chelem = true;
    // Dans quel ordre la main est tenue. Par couleur, les atouts en tête, qui
    // est l'ordre où l'on tient une main de tarot ; ou par valeur, les cartes
    // qui comptent devant, qui est l'ordre où l'on regarde sa main au moment
    // de faire son écart. Les deux boutons sont là dans les deux cas : ceci ne
    // décide que de celui qui s'applique tout seul au début de chaque donne.
    bool holdByValue = false;
    bool offerHints = true;     // le bouton Conseil, et le compte de la main
    bool celebrate = true;      // le feu d'artifice à la fin
    int pace = 1;               // 0 posé, 1 normal, 2 vif : le temps de réflexion
    // La taille des cartes, en pour cent. Un jeu installé apporte des images
    // d'une finesse donnée, et une figure réduite au quart de cela n'est plus
    // qu'un bruit de couleurs : voir TableView::setZoom().
    int zoom = 100;
    // La couleur du drap, et de tout ce qui va avec. Elle n'appartient pas à
    // la table mais au profil, qui garde la sienne : voir TableLook.h.
    int hue = 0;
    QVector<QString> invited;   // les adversaires demandés, les autres au tour de rôle
};

// La page de jeu : la table, la rangée de boutons sous elle, et ce qui fait
// jouer les trois autres.
//
// C'est ici que les deux moitiés du programme se rencontrent. En dessous, les
// règles, qui ne savent rien de l'écran ; à côté, les cerveaux, qui ne savent
// rien du temps qui passe. Cette page-là tient la partie, demande un coup à
// celui dont c'est le tour, l'attend un instant pour qu'on le voie arriver,
// laisse le chien et le pli sur la table le temps qu'on les lise, et compte
// l'expérience à la fin de chaque donne.
class TablePage : public QWidget {
    Q_OBJECT

public:
    explicit TablePage(QWidget *parent = nullptr);

    void startMatch(const GameSettings &settings);
    void applySettings(const GameSettings &settings);
    void refresh();

    const Tarot *game() const { return m_game; }
    bool playing() const;
    QString subtitle() const;

Q_SIGNALS:
    void stateChanged();
    // Demandé par la feuille de marque : on quitte la table.
    void wantsMenu();
    // Changée à la molette sur le drap : la fenêtre la garde.
    void zoomChanged(int percent);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QWidget *buildActions();

    void onTurnBegan(int seat);
    void onActed(int seat, const QString &tale);
    void onTrickFinished(int winner);
    void onDonneFinished();
    void onMatchFinished();
    void playRival();
    void sweep();
    // Le chien a été lu : il passe en main du preneur. Déclenché par le bouton
    // quand c'est à vous de prendre, par la minuterie sinon.
    void takeChien();
    // Ce que le preneur fait de sa préparation : l'écart, le chelem, et le
    // passage au jeu. Appelé pour vous comme pour un adversaire.
    void doEcart();
    void finishEcart();
    void playRivalEcart();
    // La poignée, demandée avant la première carte. Rend vrai quand il a fallu
    // poser la question, auquel cas elle a déjà été réglée.
    void askPoignee(int seat);

    void sort(bool byValue);
    // L'ordre choisi dans les réglages, posé au début de chaque donne.
    void sortAsPreferred();
    void showDealIfNew();
    void doBid(Contract contract);
    void doCall(int suit);
    void doPass();
    void doPlay();
    void doHint();
    void onCardPressed(int cardId);
    void onCardDoubleClicked(int cardId);
    void moveChoice(int step);

    void updateActions();
    // La bande du bas porte neuf boutons pendant l'enchère : c'est la phrase
    // qui cède, et elle cède proprement plutôt qu'en se faisant couper au
    // milieu d'un mot.
    void setState(const QString &text);
    void elideState();
    void sayWhatToDo();
    void grumble(const QString &why);
    int mySeat() const;
    int thinkingMs(bool first) const;
    int sweepMs() const;
    int chienMs() const;
    QVector<SeatSpec> chooseSeats() const;

    Tarot *m_game = nullptr;
    TableView *m_view = nullptr;
    ScoreBoard *m_board = nullptr;
    VictoryEffect *m_victory = nullptr;
    QWidget *m_actions = nullptr;

    QPushButton *m_sortSuit = nullptr;
    QPushButton *m_sortValue = nullptr;
    // Les quatre paroles de l'enchère, dans l'ordre où elles se dépassent.
    QPushButton *m_bid[4] = {nullptr, nullptr, nullptr, nullptr};
    QPushButton *m_call[4] = {nullptr, nullptr, nullptr, nullptr};
    QPushButton *m_pass = nullptr;
    QPushButton *m_chien = nullptr;
    QPushButton *m_ecart = nullptr;
    QPushButton *m_play = nullptr;
    QPushButton *m_hint = nullptr;
    QLabel *m_state = nullptr;

    QString m_stateText;
    GameSettings m_settings;
    QVector<Skill> m_skills;
    QTimer *m_beat = nullptr;
    QTimer *m_sweep = nullptr;
    QTimer *m_chienBeat = nullptr;
    // La donne dont la distribution a déjà été montrée. Il n'y a pas de signal
    // « on vient de donner » : c'est le numéro de la donne qui le dit, et le
    // comparer est ce qui évite de redistribuer à l'écran à chaque parole.
    int m_dealShown = -1;
    bool m_sortedDonne = false;
    bool m_donneCounted = false;
    // La poignée se demande une fois par donne et par joueur ; sans ce
    // garde-fou, la question reviendrait à chaque passage de `onTurnBegan`.
    bool m_poigneeAsked = false;
};
