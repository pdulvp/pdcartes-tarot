#pragma once

#include "Cards.h"
#include "Poignee.h"
#include "Rivals.h"
#include "Tarot.h"

#include <QString>
#include <QVector>

// Comment un adversaire décide.
//
// Un niveau n'est pas un nombre qu'on multiplie quelque part. C'est une liste
// de choses qu'un joueur fait ou ne fait pas, et chacune d'elles est une chose
// qu'on peut voir arriver de l'autre côté de la table :
//
//   prendre ou passer   Un débutant compte ses atouts et prend s'il en a
//                       beaucoup. Il ne voit pas qu'un petit sans escorte est
//                       un bout qu'on lui prendra, ni que quatre rois valent
//                       plus que six atouts moyens. À partir d'intermédiaire
//                       la main est pesée comme on la pèse à une vraie table :
//                       les bouts, la longueur d'atout, les hauts atouts, les
//                       rois, et les couleurs courtes qui feront des coupes.
//
//   l'écart             C'est le coup le plus mal joué par un débutant. Il
//                       jette ses six cartes les moins chères, une par
//                       couleur, et se retrouve long partout et sans coupe.
//                       Les autres vident une couleur entière -- celle où ils
//                       n'ont pas de roi -- pour pouvoir couper dedans tout le
//                       reste de la donne.
//
//   lire le pli         Qui tient le pli, et de quel camp il est. C'est la
//                       vraie frontière entre le débutant et les autres : le
//                       preneur est seul, les trois défenseurs jouent
//                       ensemble, et charger le pli d'un camarade quand il est
//                       acquis vaut autant que de prendre un pli soi-même. Un
//                       débutant joue sa carte comme s'il était seul à table.
//
//   compter les cartes  Un expert sait ce qui est sorti, donc ce qui reste
//                       dehors, donc si son roi est devenu maître et combien
//                       d'atouts restent à tirer. Les autres jouent avec ce
//                       qu'ils voient sur la table, ce qui n'est pas la même
//                       chose du tout.
//
//   le petit            Le petit vaut quatre points et demi et se perd au
//                       premier atout demandé si on ne le garde pas. Un joueur
//                       qui sait cela ne le joue jamais sans y être forcé, le
//                       garde pour le dernier pli quand il a de quoi le mener,
//                       et chasse celui de l'adversaire en tirant l'atout.
//
//   la bourde           Et par-dessus tout cela, un débutant joue franchement
//                       la mauvaise carte de temps en temps. Un coup sur trois
//                       tout en bas, aucun tout en haut.

struct Skill {
    qreal mastery = 0.0;
    qreal blunder = 0.0;         // la mauvaise carte, exprès
    bool readsTable = false;     // sait qui tient le pli, et de quel camp
    bool countsCards = false;    // se souvient de tout ce qui est passé
    bool weighsHand = false;     // pèse la main avant de prendre, et écarte pour couper
    bool savesPetit = false;     // protège son petit et le mène au bout
    qreal boldness = 1.0;        // avec quelle facilité il prend
};

Skill skillFor(int xp);
Skill bestSkill();               // celui avec lequel le bouton Conseil pense

// Ce que la main vaut pour prendre, en demi-points de prise.
//
// Ce n'est pas une estimation des points qu'on fera : c'est le compte que font
// les joueurs à une vraie table pour décider s'ils prennent, exprimé en demis
// afin de rester entier. Un bout vaut deux points, un roi un point, une coupe
// un point, et la longueur d'atout vaut d'autant plus qu'elle est longue.
//
// La longueur d'atout y pèse bien plus que le reste, et c'est mesuré : à une
// table de quatre, le nombre d'atouts du preneur prédit la réussite du contrat
// mieux qu'aucun autre chiffre -- six atouts tiennent une fois sur huit, dix
// une fois sur deux, et les rois ne déplacent cela que de quelques points.
//
// Sorti d'ici plutôt que caché dans le cerveau, parce que le bouton Conseil en
// dit le chiffre : savoir qu'une main vaut six points de prise et qu'il en
// faut sept pour une petite est à peu près tout ce qu'il y a à apprendre de
// l'enchère.
int handStrength(const QVector<Card> &hand);

// Ce qu'il faut, à peu près, pour chacune des quatre paroles. En demis, comme
// `handStrength`.
//
// Les quatre seuils ont été réglés en jouant, et non posés sur la
// distribution des mains : ce qui décide n'est pas la fréquence d'une main,
// c'est ce qu'elle rapporte quand on la prend. Sur quelques milliers de
// donnes, le gain moyen du preneur passe du négatif au positif autour de dix
// points de prise, et c'est là que la barre de la petite est posée -- un peu
// en dessous, pour qu'une donne sur cinq seulement soit annulée faute de
// preneur.
//
// Les trois autres barres sont bien plus hautes, et c'est voulu. La garde ne
// change rien au jeu -- le chien est pris de la même façon -- elle ne fait que
// doubler ce que la donne rapporte ou coûte : on ne la dit donc que sur une
// main qui gagne vraiment. La garde sans et la garde contre, elles, retirent
// le chien au preneur tout en multipliant par quatre et par six : il faut une
// main qui tienne son contrat sans rien recevoir, et ces mains-là sont rares.
//
// Ce que cela donne à une table de quatre, mesuré :
//
//     les quatre passent       21 %
//     petite                   83 % des donnes jouées
//     garde                    15 %
//     garde sans le chien       1,5 %
//     garde contre le chien     0,1 %
//     contrats tenus           47 %
//
inline constexpr int kPetiteStrength = 14;       // sept points de prise
inline constexpr int kGardeStrength = 24;        // douze
inline constexpr int kGardeSansStrength = 32;    // seize
inline constexpr int kGardeContreStrength = 38;  // dix-neuf

// Le contrat le plus fort que cette main justifie, sans égard à ce qui a déjà
// été annoncé. `Contract::Passe` quand elle ne justifie rien.
Contract contractFor(int strength, const Skill &skill);

// Ce qu'on dit, maintenant, à cette place. Appelé une fois par tour de parole.
Contract chooseBid(const Tarot &game, int at, const Skill &skill);

// Les six cartes qu'on met de côté. Appelé une fois, quand le preneur a le
// chien en main.
QVector<int> chooseEcart(const Tarot &game, int at, const Skill &skill);

// Montrer sa poignée, ou y renoncer. La poignée va au camp qui gagne la donne
// et non à celui qui l'a montrée : c'est un pari, et un débutant ne le voit
// pas.
PoigneeKind choosePoignee(const Tarot &game, int at, const Skill &skill);

// Annoncer le chelem. Seul le preneur peut le faire, et seulement sur une main
// qui prend les dix-huit plis.
bool chooseChelem(const Tarot &game, int at, const Skill &skill);

// La carte à jouer, maintenant. Appelée une fois par pli et par joueur.
int chooseCard(const Tarot &game, int at, const Skill &skill);

// Ce que le meilleur joueur de la table ferait de votre main, dit en français.
// C'est le bouton Conseil, et c'est le même cerveau : il n'y en a pas un
// second, meilleur, gardé pour le joueur.
QString advise(const Tarot &game, int at);
