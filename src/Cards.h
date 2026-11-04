#pragma once

#include <QString>
#include <QVector>

// Ce qu'est une carte de tarot, et ce qu'elle vaut.
//
// Soixante-dix-huit cartes : quatre enseignes de quatorze -- de l'as au roi,
// avec le cavalier glissé entre le valet et la dame -- vingt et un atouts
// numérotés de 1 à 21, et l'excuse. Chaque carte est unique dans la donne,
// comme à la belote : une carte jouée est sortie pour de bon, et c'est ce qui
// rend le comptage possible.
//
// L'atout n'est pas une enseigne qu'on désigne au début de la donne : c'est
// une cinquième famille, imprimée sur les cartes, et elle est la même à toutes
// les donnes. Rien ici ne demande donc quel est l'atout, à la différence de la
// belote où aucune carte n'avait de valeur avant qu'on l'ait dit.
//
// Les points se comptent en demi-points entiers. Une carte de tarot vaut 4,5,
// 3,5, 2,5, 1,5 ou 0,5 point, et la donne en vaut 91 : écrire ces valeurs en
// nombres à virgule obligerait à comparer des flottants pour savoir si un
// contrat est tenu. Tout ce qui est un point dans ce programme est donc un
// demi-point entier -- le roi vaut 9, la donne 182, le contrat à deux bouts 82
// -- et c'est l'affichage qui divise par deux, par `pointsText()`.

enum class Suit {
    Pique = 0,
    Coeur = 1,
    Carreau = 2,
    Trefle = 3,
    // La cinquième famille. Elle porte aussi l'excuse, qui est un atout par
    // la place qu'elle occupe dans la main et nullement par ce qu'elle fait :
    // l'excuse ne prend jamais un pli.
    Atout = 4,
};

// Les quatre enseignes, et les atouts par-dessus.
inline constexpr int kSuitCount = 4;
inline constexpr int kTrumpCount = 21;

// Les rangs d'une enseigne, dans l'ordre où ils prennent les plis. L'as est en
// bas et le roi en haut : au tarot l'as ne vaut rien et ne prend rien, ce qui
// est l'inverse de presque tous les autres jeux.
inline constexpr int kAs = 1;
inline constexpr int kValet = 11;
inline constexpr int kCavalier = 12;
inline constexpr int kDame = 13;
inline constexpr int kRoi = 14;
inline constexpr int kRanksInSuit = kRoi;

// L'excuse porte le rang zéro dans la famille des atouts : elle n'a pas de
// numéro, et zéro est la seule place libre sous le petit.
inline constexpr int kExcuse = 0;
// Les deux atouts qui sont des bouts. Le troisième est l'excuse.
inline constexpr int kPetit = 1;
inline constexpr int kVingtEtUn = 21;

struct Card {
    int id = -1;          // laquelle des soixante-dix-huit, pour l'écran
    int rank = kAs;
    Suit suit = Suit::Pique;

    bool valid() const { return id >= 0; }
    bool trump() const { return suit == Suit::Atout; }
    bool excuse() const { return suit == Suit::Atout && rank == kExcuse; }
};

// Les cartes sont numérotées de 0 à 77 : quatre enseignes de quatorze, puis
// les vingt et un atouts, puis l'excuse. Cette numérotation est ce qui permet
// de se souvenir de ce qui est passé avec un tableau de soixante-dix-huit
// cases plutôt qu'avec une liste.
inline constexpr int kPackSize = 78;
inline constexpr int kExcuseId = kPackSize - 1;

inline int cardIdFor(int rank, Suit suit) {
    if (suit != Suit::Atout) return int(suit) * kRanksInSuit + (rank - kAs);
    if (rank == kExcuse) return kExcuseId;
    return kSuitCount * kRanksInSuit + (rank - 1);
}

inline bool operator==(const Card &a, const Card &b) { return a.id == b.id; }

// Le jeu : les soixante-dix-huit, dans l'ordre. Le mélange est le seul hasard
// qu'il y ait dans une partie.
QVector<Card> freshPack();
void shuffleCards(QVector<Card> &cards);

// Le tarot se joue à 3, 4 ou 5 joueurs. Quatre est la valeur ordinaire et
// celle qui reste écrite dans les réglages d'avant ; le nombre de sièges
// d'une partie se lit dans `Tarot::seatCount()`, et tout ce qui en dépend
// passe par les fonctions ci-dessous.
inline constexpr int kSeatCount = 4;    // le nombre de joueurs par défaut
inline constexpr int kMinSeats = 3;
inline constexpr int kMaxSeats = 5;     // borne des tableaux de sièges

// La taille d'une main et celle du chien selon le nombre de joueurs, au
// règlement FFT : 24 cartes et 6 au chien à 3 joueurs (distribuées 4 par 4),
// 18 et 6 à 4 (3 par 3), 15 et 3 à 5 (3 par 3). Dans les trois cas le jeu
// de soixante-dix-huit cartes est épuisé à la fin de la donne.
int handSizeFor(int seats);    // 24, 18, 15
int chienSizeFor(int seats);   // 6, 6, 3
int dealPacketFor(int seats);  // paquet de distribution : 4 à 3 joueurs, 3 sinon

// Le nombre de plis d'une donne : autant que de cartes en main.
inline int trickCountFor(int seats) { return handSizeFor(seats); }

// Ce que vaut la donne, en demi-points.
//
// Quatre-vingt-onze points, soit cent quatre-vingt-deux demis : trente-quatre
// par enseigne, trente-sept dans les atouts, et neuf pour l'excuse.
inline constexpr int kDonnePoints = 182;

// Les deux camps : le preneur, seul, et les trois autres. Ce n'est pas un
// réglage -- c'est le tarot à quatre, où l'on ne choisit pas son camp mais où
// l'on s'y met tout seul en prenant.
inline constexpr int kTeamCount = 2;
inline constexpr int kTakerTeam = 0;
inline constexpr int kDefenceTeam = 1;

// Ce qu'il faut ramasser selon le nombre de bouts qu'on a dans son tas :
// 56 points sans bout, 51 avec un, 41 avec deux, 36 avec les trois. En
// demi-points, et indexé par le nombre de bouts.
inline constexpr int kBoutCount = 3;
inline constexpr int kTargets[kBoutCount + 1] = {112, 102, 82, 72};
int targetFor(int bouts);

// Un bout : le petit, le vingt et un, l'excuse. Ce sont les trois cartes qui
// abaissent le contrat, et les trois qu'on ne peut pas écarter.
bool isBout(const Card &card);

bool isRed(Suit suit);
QString suitPip(Suit suit);            // le symbole, « ♠ » ou « ✦ » pour l'atout
QString suitName(Suit suit);           // « pique », « atout », pour une phrase
QString rankLabel(int rank, Suit suit);// « 7 », « R », « 17 », « Exc »
QString cardLabel(const Card &card);   // « R♥ », « 17✦ », pour une place étroite
QString cardName(const Card &card);    // « le roi de cœur », « l'atout 17 »

// Des demi-points tels qu'on les lit : « 41 », « 47,5 ».
QString pointsText(int halves);

// Un nombre et son nom accordé : « 1 donne », « 4 donnes », « 0 bout ».
//
// Qt sait accorder tout seul, par `tr("%n ...", nullptr, n)`, mais seulement
// lorsqu'un catalogue de traduction est chargé : sans catalogue il remplace
// `%n` et laisse le « (s) » dans la phrase. Le jeu n'est pas traduit -- il est
// écrit en français -- et l'accord se fait donc ici. Zéro et un prennent le
// singulier, comme il se doit en français.
//
// Elle est posée à côté de `pointsText()`, les deux étant les deux seules
// façons qu'a le programme d'écrire un nombre.
QString countedText(int count, const QString &one, const QString &many);

// Ce que la carte rapporte à celui qui ramasse le pli, en demi-points. Une
// seule table, et elle ne dépend de rien : au tarot une carte vaut ce qu'elle
// vaut, atout ou non.
int cardPoints(const Card &card);

// La force d'une carte pour prendre un pli. Les atouts passent tous devant les
// enseignes, et l'excuse ne passe devant rien.
int cardStrength(const Card &card);

// `candidate` prend-elle le pli à `best`, la couleur demandée étant `led` ?
// L'atout coupe tout le reste, une carte d'une autre enseigne que celle
// demandée ne prend rien, et l'excuse ne prend jamais.
bool beatsTrick(const Card &candidate, const Card &best, Suit led);

// Ranger la main.
//
// Par couleur : les atouts en tête, du vingt et un au petit, puis les quatre
// enseignes du roi vers l'as. C'est ainsi qu'on tient une main de tarot.
//
// Par valeur : ce qui compte d'abord -- les bouts, les rois, les dames, les
// cavaliers, les valets -- et les cartes blanches à la suite. C'est l'ordre
// dans lequel on regarde sa main au moment de faire son écart, qui est le seul
// moment où la question « qu'est-ce que je peux jeter » se pose.
void sortBySuit(QVector<Card> &cards);
void sortByValue(QVector<Card> &cards);

Card takeCard(QVector<Card> &cards, int id);
const Card *findCard(const QVector<Card> &cards, int id);
bool hasSuit(const QVector<Card> &cards, Suit suit);
int countSuit(const QVector<Card> &cards, Suit suit);
// La somme des demi-points d'un tas.
int totalPoints(const QVector<Card> &cards);
int countBouts(const QVector<Card> &cards);
