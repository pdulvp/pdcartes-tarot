#pragma once

#include "Cards.h"

#include <QString>
#include <QVector>

// La poignée : les atouts qu'une main montre avant de les jouer.
//
// C'est la seule annonce du tarot, et elle ne ressemble pas à celles de la
// belote. On ne la compare à rien : elle n'est pas disputée entre les deux
// camps, elle est simplement montrée, et ce qu'elle vaut est ajouté à la fin
// au camp qui gagne la donne -- celui qui l'a montrée aussi bien que l'autre.
// Montrer une poignée est donc un pari sur l'issue de la donne autant qu'un
// renseignement donné à la table.
//
// Elle se montre avant de jouer sa première carte, et elle se montre en
// entier : les dix, treize ou quinze atouts sont posés sur le tapis, vus de
// tous, puis repris en main. Trois joueurs sur quatre apprennent ainsi d'un
// coup où sont les atouts, ce qui est cher payé pour vingt points quand on
// est du mauvais côté.
//
// L'excuse peut compléter une poignée, mais à une seule condition : que le
// joueur n'ait pas un atout de plus à montrer à sa place. Montrer l'excuse
// dans une poignée, c'est donc dire « je n'ai rien d'autre », et la table le
// sait.

enum class PoigneeKind {
    Aucune = 0,
    Simple,   // dix atouts
    Double,   // treize
    Triple,   // quinze
};

// Les tailles au tarot à quatre ; à 3 joueurs ce sont 13/15/18 atouts, et à
// 5 joueurs 8/10/13 (règlement FFT).
inline constexpr int kSimpleTrumps = 10;
inline constexpr int kDoubleTrumps = 13;
inline constexpr int kTripleTrumps = 15;

// Ce qu'elles valent, en demi-points : vingt, trente et quarante points.
inline constexpr int kSimplePoints = 40;
inline constexpr int kDoublePoints = 60;
inline constexpr int kTriplePoints = 80;

// Combien d'atouts il faut pour celle-là, selon le nombre de joueurs, et ce
// qu'elle vaut.
int poigneeTrumps(PoigneeKind kind, int seats = kSeatCount);
int poigneePoints(PoigneeKind kind);
QString poigneeName(PoigneeKind kind);     // « simple poignée »

// Les atouts d'une main, l'excuse comprise : c'est ce qu'on pose sur le tapis.
// Le compte sert aussi au cerveau, qui mesure la longueur d'atout par là.
int trumpsInHand(const QVector<Card> &hand);

// La plus grosse poignée que cette main puisse montrer.
//
// L'excuse n'est comptée que pour arriver juste au compte : une main de dix
// atouts plus l'excuse montre une simple poignée faite de ses dix atouts, et
// garde l'excuse ; une main de neuf atouts plus l'excuse montre les neuf et
// l'excuse, et dit par là qu'elle n'a pas de dixième.
PoigneeKind bestPoignee(const QVector<Card> &hand, int seats = kSeatCount);

// Les cartes qu'elle met sur le tapis : les plus hauts atouts, et l'excuse en
// dernier lorsqu'elle est nécessaire pour atteindre le compte.
QVector<Card> poigneeCards(const QVector<Card> &hand, PoigneeKind kind,
                           int seats = kSeatCount);
