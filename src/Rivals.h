#pragma once

#include <QString>
#include <QVector>

// Les six adversaires, et ce qu'ils ont appris.
//
// Ce ne sont pas six réglages d'une même difficulté qu'on tourne. Chacun est
// un joueur avec un passé : deux s'assoient en ne sachant presque rien, trois
// connaissent le jeu, et Oriane y joue depuis des années. Et ils continuent de
// jouer après qu'on a rangé le jeu -- ou plutôt, ce qu'ils ont appris des
// parties qu'on a faites est encore là quand on revient, de sorte qu'une table
// qu'on battait en mars est une table qui vous bat en juin.
//
// Ce qu'ils savent est un seul chiffre, leur expérience, qui monte un peu à
// chaque donne jouée et un peu plus à chaque donne gagnée. Trois noms sont
// posés sur ses plages, parce que « niveau 437 » n'est pas une chose qu'on a
// envie de lire :
//
//     débutant        0 - 199     compte ses atouts et prend là-dessus, écarte
//                                 au hasard, joue sa carte sans regarder qui
//                                 tient le pli, et se trompe franchement de
//                                 temps en temps
//     intermédiaire 200 - 599     pèse sa main avant de prendre, écarte pour
//                                 se faire une coupe, protège son petit, et
//                                 charge le camp qui tient le pli
//     expert        600 - 1200    compte les soixante-dix-huit cartes, sait
//                                 combien d'atouts restent dehors, tire
//                                 l'atout quand il a pris, chasse le petit de
//                                 l'adversaire et garde le sien pour le bout
//
// Trois d'entre eux s'assoient à chaque partie. Au tarot à quatre il n'y a pas
// de partenaire fixe : le preneur est seul contre les trois autres, et les
// camps sont refaits à chaque donne. Chacun est gardé par profil, de sorte que
// changer de profil est changer de table -- et chacun peut être remis au
// niveau avec lequel il est arrivé, un à la fois, d'un clic droit sur sa carte
// au menu d'ouverture -- et rebaptisé du même clic droit.

enum class Level { Debutant, Intermediaire, Expert };

inline constexpr int kMaxExperience = 1200;

struct Rival {
    QString id;
    QString name;
    QString blurb;       // ce qu'il est à table, en une ligne
    int hue;             // sa couleur, portée sur sa plaque et sur ses cartes
    int startXp;         // l'expérience avec laquelle il s'assoit, et où on le remet
    int emblem;          // la marque de son médaillon, de 0 à 5
};

const QVector<Rival> &rivals();
const Rival &rivalById(const QString &id);

// Ce qu'il a fait sous ce profil.
struct Learning {
    int xp = 0;
    int donnesPlayed = 0;
    int donnesWon = 0;        // les donnes où son camp a marqué le plus
    int contractsMade = 0;    // les donnes qu'il a prises et réussies
    int matchesPlayed = 0;
    int matchesWon = 0;
};

Learning learningFor(const QString &rivalId);
Learning learningFor(const QString &rivalId, int slot);

// Le nom sous lequel on joue avec lui, ou contre lui, sur ce profil-là.
//
// Les six restent les six : même couleur, même emblème, même cerveau, même
// expérience. Seul le nom change, et il change par profil, comme tout ce qui
// appartient à un profil -- la table du dimanche peut avoir ses habitués sans
// que celle de la semaine les connaisse. Sans nom donné, c'est celui avec
// lequel il est arrivé.
QString rivalName(const QString &rivalId);
QString rivalName(const QString &rivalId, int slot);
void setRivalName(const QString &rivalId, const QString &name);
void clearRivalName(const QString &rivalId);

// Au plus long qu'un nom donné puisse être : il faut qu'il tienne sur la
// plaque de la table, à côté de ce que le joueur vient de dire.
inline constexpr int kRivalNameLength = 18;

// Compté à la fin de chaque donne. Une donne jouée vaut un peu quoi qu'il y
// soit arrivé, une donne gagnée un peu plus, et un contrat pris et tenu plus
// encore : c'est la seule des trois qui demande d'avoir décidé quelque chose.
void noteRivalDonne(const QString &rivalId, bool teamWon, bool wasTaker, bool madeContract);
void noteRivalMatch(const QString &rivalId, bool won);

// Au niveau avec lequel celui-là s'assoit, sur ce profil seulement. Le
// niveau de départ se demande par joueur et non pour les six d'un coup : un
// joueur qui a rendu Oriane imbattable peut vouloir que Lulu reste débutant.
void resetRival(const QString &rivalId);

Level levelOf(int xp);
QString levelName(Level level);
int levelStars(Level level);           // une, deux ou trois
int levelFloor(Level level);
int levelCeiling(Level level);

// De zéro à un sur toute la plage : ce qu'on donne au cerveau, et ce à quoi la
// jauge de sa carte est remplie.
qreal masteryOf(int xp);

// De zéro à un à l'intérieur du niveau où il est, pour la jauge sous le nom.
qreal progressWithin(int xp);
