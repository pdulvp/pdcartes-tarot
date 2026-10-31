#pragma once

#include "Cards.h"

// Le dessin d'une carte vit dans la cartothèque : les deux programmes qui la
// partagent en ont l'usage, l'un pour jouer, l'autre pour fabriquer des jeux.
// Ce fichier l'inclut plutôt que de laisser chaque appelant le faire,
// `plainFont()` et `cardCornerFor()` étant demandées un peu partout dans
// l'application.
#include "CardArt.h"
#include "Palette.h"

#include <QPixmap>
#include <QRectF>
#include <QSizeF>

class QPainter;
class QFont;

// Le dessin du tapis, et des cartes qui y sont posées.
//
// Ce qui se trouve ici ne concerne que la table : le tissu, les plaques
// posées dessus, les médaillons des adversaires, et les quelques réglages
// d'une carte qui dépendent de la table -- l'ombre qu'elle y porte, le liseré
// de sélection, le halo du coup qui vient d'être joué, et la substitution
// d'un jeu installé à la carte dessinée.

// La proportion des cartes en cours : celle du jeu installé s'il y en a un,
// celle du jeu dessiné sinon.
qreal cardAspect();
QSizeF cardSizeForHeight(qreal height);
QSizeF cardSizeForWidth(qreal width);

// Le tapis : le tissu, éclairé en son centre, dont la trame est visible de
// près, bordé d'un galon.
//
// `braid` commande le galon. On le laisse tomber quand le drap n'est pas une
// table mais un fond : le menu d'ouverture est posé dessus, et un galon
// derrière une page de plaques est un cadre de plus qui ne cadre rien.
void drawFelt(QPainter *painter, const QRectF &box, const Palette &scheme, bool braid = true);

// Ce qu'on pose sur le drap, et ce qui le borde.
//
// Écrit ici et non dans chaque page, parce que le menu d'ouverture et la
// table portent la même tenue et qu'une recette répétée est une recette qui
// se met à diverger. Presque noir et translucide, pour que le tissage se
// devine dessous -- c'est ce qui fait que la plaque est posée sur le tapis et
// non découpée dedans.
//
// `lift` tire la plaque vers l'or quand elle est choisie ou allumée.
QColor feltPlaqueFill(const Palette &scheme, qreal lift = 0.0);
// L'or des filets, à la force qu'on veut : un cheveu au repos, franc quand la
// chose est choisie.
QColor feltGold(const Palette &scheme, qreal strength);
// Le fond des deux bandes, celle du haut et celle du bas : plus sombre encore
// que les plaques, parce qu'une bande n'est pas posée sur le drap -- elle le
// borde.
QColor feltBarFace(const Palette &scheme);

// L'état de rendu d'une carte face visible. `lit` désigne la carte survolée
// par le pointeur, `chosen` la carte sélectionnée dans la main du joueur,
// `glow` celle qui vient d'être jouée -- mise en évidence sans laquelle les
// coups des trois autres joueurs seraient difficiles à suivre -- et `dimmed`
// une carte que les règles ne laissent pas jouer maintenant, ce qui est la
// façon d'enseigner les règles du tarot sans un mot.
struct CardLook {
    bool lit = false;
    bool chosen = false;
    qreal glow = 0.0;       // de zéro à un, en fondu
    bool dimmed = false;    // carte non jouable dans l'état courant
    bool noShadow = false;  // pour dessiner une carte hors du tapis
};

void drawCardFace(QPainter *painter, const QRectF &box, const Card &card, const Palette &scheme,
                  const CardLook &look = CardLook{});

// Une carte face cachée : le dos, dans la couleur de la palette.
void drawCardBack(QPainter *painter, const QRectF &box, const Palette &scheme,
                  const CardLook &look = CardLook{});

// La place vide : celle où une carte va être posée et ne l'est pas encore. Un
// contour en pointillés avec un mot dedans, pour qu'une place vide dise
// quand même à qui elle est -- ce qui fait de quatre rectangles une table
// avec du monde autour.
void drawCardSlot(QPainter *painter, const QRectF &box, const Palette &scheme,
                  const QString &label);

// Le symbole d'une couleur, désignée par le type de l'application plutôt que
// par le numéro qu'emploie la cartothèque.
inline void drawSuitPip(QPainter *painter, const QRectF &box, Suit suit, const QColor &ink) {
    drawSuitPip(painter, box, int(suit), ink);
}

// Le médaillon d'un joueur : un anneau à sa couleur, contenant un symbole et
// son initiale. Chacun a le sien, de sorte qu'une plaque se reconnaisse à sa
// forme autant qu'à sa couleur.
void drawRivalEmblem(QPainter *painter, const QRectF &box, int emblem, const QString &initial,
                     const QColor &ink, const QColor &back);

// Les étoiles du niveau : une pour un débutant, deux pour un intermédiaire,
// trois pour un expert.
void drawLevelStars(QPainter *painter, const QRectF &box, int stars, int outOf,
                    const QColor &ink, const QColor &empty);

// Une jauge indiquant la progression à l'intérieur du niveau.
void drawGauge(QPainter *painter, const QRectF &box, qreal filled, const QColor &ink,
               const QColor &back);

// Un petit tas de cartes face cachée : la main d'un adversaire, vue depuis
// l'autre côté de la table. `cards` est le nombre de cartes qu'il détient.
void drawHandPile(QPainter *painter, const QRectF &box, int cards, const Palette &scheme,
                  const QColor &tint);
