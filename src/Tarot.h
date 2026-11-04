#pragma once

#include "Cards.h"
#include "Poignee.h"

#include <QObject>
#include <QString>
#include <QVector>

// La donne : les règles, et rien d'autre.
//
// Rien ici ne sait qu'il y a un écran, ni que trois des quatre joueurs ne sont
// pas des gens. Ce fichier tient les soixante-dix-huit cartes, l'enchère, le
// chien, l'écart, le pli en cours et la marque -- et il refuse un coup que le
// tarot ne permet pas, avec une phrase qui dit pourquoi, parce qu'une règle
// qu'on applique en silence est une règle que personne n'apprend.
//
// Le tarot n'est pas un jeu à deux camps fixés. Un joueur prend, et il est
// seul contre les trois autres pour cette donne-là : les camps sont refaits à
// chaque donne et personne ne choisit le sien. C'est pourquoi la marque est
// individuelle -- chacun a son total -- alors que les points des plis se
// comptent par camp.
//
// Le déroulement d'une donne :
//
//   la donne      dix-huit cartes à chacun par paquets de trois, et six au
//                 chien. Une main dont le petit est le seul atout fait
//                 redonner : c'est le petit sec.
//
//   l'enchère     chacun à son tour passe ou annonce un contrat plus fort que
//                 le précédent : petite, garde, garde sans le chien, garde
//                 contre le chien. Si les quatre passent, la donne est
//                 annulée.
//
//   le chien      à la petite et à la garde, il est retourné, vu de tous, et
//                 pris par le preneur, qui écarte six cartes à la place. À la
//                 garde sans, il va au tas du preneur sans être vu ; à la
//                 garde contre, à celui des défenseurs.
//
//   les plis      dix-huit. On fournit, à défaut on coupe, et sur un atout
//                 déjà posé on monte si on peut. L'excuse se joue quand on
//                 veut et ne prend jamais le pli.
//
//   la marque     le preneur doit ramasser 56, 51, 41 ou 36 points selon le
//                 nombre de bouts qu'il a dans son tas. L'écart au contrat
//                 décide de tout le reste.

enum class Phase {
    Idle,
    Bidding,
    // À 5 joueurs, le preneur appelle un roi avant de voir le chien : le
    // détenteur de ce roi sera son partenaire pour la donne.
    Appel,
    // Le chien retourné, posé sur le tapis et vu de tous. Une phase à part
    // parce qu'elle s'arrête : tout le monde doit avoir le temps de le lire.
    Chien,
    // Le preneur choisit les six cartes qu'il met de côté.
    Ecart,
    Playing,
    TrickOver,
    DonneOver,
    MatchOver,
};

// Les cinq paroles de l'enchère, dans l'ordre où elles se dépassent.
enum class Contract { Passe = 0, Petite, Garde, GardeSans, GardeContre };

inline constexpr int kContractCount = 5;

// Par combien la donne est multipliée : un, deux, quatre, six.
int contractMultiplier(Contract contract);
QString contractName(Contract contract);    // « garde sans le chien »
QString contractShort(Contract contract);   // « garde sans »
// Le preneur voit-il le chien et écarte-t-il ? Vrai pour la petite et la
// garde, faux pour les deux autres.
bool contractTakesChien(Contract contract);

// Ce que le chelem vaut, en demi-points, selon ce qui en a été fait.
inline constexpr int kChelemAnnonceReussi = 800;   // 400 points
inline constexpr int kChelemAnnonceManque = 400;   // 200 points perdus
inline constexpr int kChelemSansAnnonce = 400;     // 200 points
// Le petit au bout : dix points, multipliés comme le reste du contrat.
inline constexpr int kPetitAuBout = 20;
// Ce que vaut une donne avant tout écart : vingt-cinq points.
inline constexpr int kDonneBase = 50;

struct SeatSpec {
    QString rivalId;     // vide pour vous
    QString name;
    int hue = 0;
};

struct Seat {
    QString rivalId;
    QString name;
    int hue = 0;

    QVector<Card> hand;
    // Ce qu'il a dit à l'enchère, et sous quelle forme on l'affiche.
    Contract bid = Contract::Passe;
    bool passed = false;
    QString said;

    // La poignée qu'il a montrée, et s'il a déjà eu l'occasion de le faire.
    PoigneeKind poignee = PoigneeKind::Aucune;
    QVector<Card> poigneeShown;
    bool declared = false;

    bool human() const { return rivalId.isEmpty(); }
};

// Une carte sur la table, et par qui.
struct Played {
    int seat = -1;
    Card card;
};

// Ce que la donne a donné, une fois comptée. Rempli d'un coup à la fin, pour
// la feuille de marque : c'est la seule chose qu'on regarde entre deux donnes,
// et elle doit tout dire sans qu'on ait à se souvenir de la donne.
struct DonneResult {
    bool played = false;
    bool annulled = false;         // les quatre ont passé

    Contract contract = Contract::Passe;
    int taker = -1;
    int callee = -1;      // à 5 joueurs, le partenaire appelé ; -1 sinon

    int points[kTeamCount] = {0, 0};   // les demi-points ramassés
    int tricks[kTeamCount] = {0, 0};
    int bouts = 0;                 // les bouts du preneur
    int target = 0;                // ce qu'il lui fallait
    int ecart = 0;                 // points réalisés moins contrat, signé
    bool made = false;

    int petitAuBoutTeam = -1;      // le camp qui a mené le petit au dernier pli
    int poigneePoints = 0;         // la somme des poignées montrées
    PoigneeKind poignee = PoigneeKind::Aucune;
    int poigneeSeat = -1;
    int chelem = 0;                // en demi-points, du côté du preneur
    bool chelemAnnounced = false;
    bool chelemMade = false;

    // Ce qu'un défenseur perd ou gagne, et donc le tiers de ce que le preneur
    // fait. En demi-points, du point de vue du preneur.
    int unit = 0;
    int gained[kMaxSeats] = {0, 0, 0, 0, 0};
};

class Tarot : public QObject {
    Q_OBJECT

public:
    explicit Tarot(QObject *parent = nullptr);

    // `donnes` est la longueur de la partie, comptée en donnes jouées ; les
    // donnes annulées ne comptent pas. `withPoignees` et `withChelem` sont des
    // règles et non des commodités : elles ne changent pas une partie en
    // cours.
    void startMatch(const QVector<SeatSpec> &seats, int donnes, bool withPoignees,
                    bool withChelem);
    void startDonne();

    int seatCount() const { return m_seats.size(); }
    const Seat &seat(int at) const;
    const QVector<Seat> &seats() const { return m_seats; }
    // La donne en cours, selon le nombre de joueurs : cartes en main, cartes
    // au chien, plis.
    int handSize() const { return handSizeFor(seatCount()); }
    int chienSize() const { return chienSizeFor(seatCount()); }
    int trickCount() const { return trickCountFor(seatCount()); }

    bool withPoignees() const { return m_withPoignees; }
    bool withChelem() const { return m_withChelem; }

    // Les camps de cette donne-là : le preneur d'un côté, les autres de
    // l'autre — sauf à 5 joueurs avec un appel, où le partenaire appelé est
    // avec le preneur. Pendant l'enchère il n'y a pas encore de camps, et
    // tout le monde est compté défenseur.
    int teamOf(int at) const {
        if (at < 0) return kDefenceTeam;
        if (at == m_taker) return kTakerTeam;
        if (m_seats.size() == 5 && at == m_callee) return kTakerTeam;
        return kDefenceTeam;
    }
    // Le camp de `at` tel que `observer` a le droit de le connaître : quand
    // l'appel est caché et pas encore révélé, l'appelé n'est connu que de
    // lui-même (il tient la carte appelée et le sait) ; pour tous les autres
    // il compte défenseur, comme à l'enchère.
    int knownTeamOf(int at, int observer) const {
        if (m_seats.size() == 5 && at == m_callee && observer != m_callee &&
            m_callHidden && !m_callRevealed) {
            return kDefenceTeam;
        }
        return teamOf(at);
    }
    static int otherTeam(int team) { return team ^ 1; }

    Phase phase() const { return m_phase; }
    int turn() const { return m_turn; }
    int dealer() const { return m_dealer; }
    int donneNumber() const { return m_donneNumber; }   // celles qui ont été jouées
    int targetDonnes() const { return m_donnes; }
    // Combien de fois on a distribué depuis le début de la partie. Ce n'est
    // pas le numéro de la donne : une donne annulée fait redonner sans rien
    // ajouter à la marque, et c'est ce compte-ci qui dit à l'écran qu'il y a
    // une nouvelle distribution à montrer.
    int dealsGiven() const { return m_dealsGiven; }
    // Combien de fois il a fallu redonner pour cause de petit sec, cette
    // donne-ci. Dit à la table, parce qu'un jeu qui se remélange sans rien
    // dire a l'air de bégayer.
    int redeals() const { return m_redeals; }

    // --- L'enchère.

    Contract bid() const { return m_bid; }
    int bidder() const { return m_bidder; }
    // Le contrat le plus faible qu'on puisse encore annoncer.
    Contract lowestBid() const;
    bool mayBid(Contract contract) const;
    bool makeBid(Contract contract, QString *why = nullptr);
    bool pass(QString *why = nullptr);

    int taker() const { return m_taker; }
    // --- L'appel du roi, à 5 joueurs.

    // Le preneur peut-il appeler, et quelles couleurs peut-il appeler ?
    bool mayCall(int at) const;
    // La carte appelée pour une couleur : le premier rang (roi, dame,
    // cavalier, valet) que le preneur ne détient pas en entier.
    Card calledCardFor(Suit suit, int at) const;
    // Appelle : pose le rang appelé, trouve le détenteur (chien ou preneur
    // -> personne ne répond, le preneur joue seul), et passe à la suite.
    bool callKing(int at, Suit suit, QString *why = nullptr);
    int callee() const { return m_callee; }
    Suit calledSuit() const { return m_calledSuit; }
    // L'appel peut être caché : personne ne sait qui est l'appelé tant que la
    // carte appelée n'a pas été jouée (où elle se découvre, publiquement).
    void setCallHidden(bool hidden) { m_callHidden = hidden; }
    bool callHidden() const { return m_callHidden; }
    bool callRevealed() const { return m_callRevealed; }
    // La devinette : à 5 joueurs avec l'appel caché et pas encore révélé, le
    // siège qui semble être le partenaire du preneur, d'après ce qui a été
    // joué en public — chaque pli ramassé par le preneur alimente le soupçon
    // de qui y a posé des points. Renvoie -1 tant qu'aucun soupçon, et
    // m_callee dès que l'appel est public (ou hors appel).
    int guessedCallee() const;
    Contract contract() const { return m_contract; }
    int multiplier() const { return contractMultiplier(m_contract); }

    // --- Le chien et l'écart.

    // Le chien, tant qu'il est sur la table. Vidé dès qu'il est pris.
    const QVector<Card> &chien() const { return m_chien; }
    // Le chien a été lu : on passe à l'écart, ou directement au jeu.
    void chienSeen();

    // Le preneur a-t-il six cartes à mettre de côté ? Faux à la garde sans et
    // à la garde contre, où la phase de l'écart ne sert plus qu'à laisser
    // annoncer le chelem.
    bool ecartNeeded() const { return m_ecartNeeded; }
    // L'écart est-il réglé, et peut-on passer au jeu ?
    bool ecartDone() const { return !m_ecartNeeded || !m_ecart.isEmpty(); }
    // Passer de la préparation aux dix-huit plis. Appelée une fois l'écart
    // fait et le chelem annoncé ou non.
    void finishEcart();

    // Les cartes que le preneur peut mettre de côté. Ni roi ni bout ; des
    // atouts seulement quand il n'y a pas six autres cartes à jeter.
    bool mayDiscard(const Card &card) const;
    // Combien d'atouts l'écart est obligé de contenir. Zéro presque toujours.
    int forcedTrumpsInEcart() const;
    // Les six cartes mises de côté. Elles comptent pour le preneur à la fin.
    bool setEcart(const QVector<int> &cardIds, QString *why = nullptr);
    const QVector<Card> &ecart() const { return m_ecart; }

    // Le chelem : annoncé par le preneur une fois son écart fait, et par lui
    // seul. L'annonce donne l'entame au preneur.
    bool mayAnnounceChelem(int at) const;
    bool announceChelem(QString *why = nullptr);
    bool chelemAnnounced() const { return m_chelemAnnounced; }

    // --- La poignée.

    // La poignée que cette place peut montrer, si elle en a une et si la
    // partie les compte.
    PoigneeKind poigneeAvailable(int at) const;
    // Montrer, ou renoncer. Appelée une fois par joueur, avant sa première
    // carte ; `PoigneeKind::Aucune` est un renoncement.
    bool showPoignee(int at, PoigneeKind kind);
    bool mayShowPoignee(int at) const;

    // --- Le jeu.

    const QVector<Played> &trick() const { return m_trick; }
    const QVector<Played> &lastTrick() const { return m_lastTrick; }
    // Tout ce qui est passé sur la table depuis le début de la donne, dans
    // l'ordre. C'est public : un expert est un joueur qui s'en souvient, pas
    // un joueur qui a le droit de regarder votre main.
    const QVector<Played> &history() const { return m_history; }

    int trickNumber() const { return m_trickNumber; }   // de un à dix-huit
    // L'enseigne demandée. Quand l'excuse a entamé, c'est la carte suivante
    // qui la fixe, et elle n'est donc connue qu'à partir de là.
    Suit ledSuit() const;
    bool ledKnown() const;
    bool trickStarted() const { return !m_trick.isEmpty(); }
    // La place qui tient le pli en cours, ou -1 quand il est vide ou qu'il n'y
    // a encore que l'excuse dessus.
    int trickMaster() const;
    int trickWinner() const { return m_trickWinner; }
    // Ce que le pli en cours vaut déjà, en demi-points.
    int trickValue() const;

    // Ce qu'on a le droit de jouer, et pourquoi pas. La liste est la règle :
    // `legal` ne fait que la consulter et mettre une phrase sur un refus.
    QVector<int> legalCards(int at) const;
    bool legal(int at, int cardId, QString *why = nullptr) const;

    bool play(int cardId, QString *why = nullptr);
    // Le pli reste sur la table une fois la quatrième carte posée : il faut le
    // ramasser pour que la donne avance. C'est ce qui laisse le temps de voir
    // qui a pris quoi.
    void sweepTrick();

    // --- La marque.

    int seatScore(int at) const;            // le total de la partie
    int teamPoints(int team) const;         // les demi-points ramassés
    int teamTricks(int team) const;
    // Les bouts que le preneur tient à cet instant, et ce qu'il lui faut.
    int takerBouts() const;
    int takerTarget() const;
    // La place qui mène, ou -1 si plusieurs sont à égalité en tête.
    int matchWinner() const { return m_matchWinner; }
    QString teamName(int team) const;
    const DonneResult &result() const { return m_result; }

    // Ranger une main. L'ordre dans lequel on tient ses cartes appartient à
    // celui qui les tient, alors il vit ici avec la main.
    void sortSeatHand(int at, bool byValue);

Q_SIGNALS:
    void changed();
    // Une ligne de ce qui vient de se passer, pour la bande au-dessus de la
    // main : le seul récit suivi d'une partie jouée par trois autres
    // personnes.
    void acted(int seat, const QString &tale);
    void turnBegan(int seat);
    void trickFinished(int winner);
    void donneFinished();
    void matchFinished();

private:
    void deal();
    bool petitSec() const;
    void nextBidder();
    void settleBidding();
    void annulDonne();
    void takeChien();
    void beginPlay();
    void finishDonne();
    Seat &here();

    QVector<Seat> m_seats;
    QVector<Card> m_chien;
    QVector<Card> m_ecart;

    QVector<Played> m_trick;
    QVector<Played> m_lastTrick;
    QVector<Played> m_history;

    bool m_withPoignees = true;
    bool m_withChelem = true;
    Phase m_phase = Phase::Idle;
    int m_turn = 0;
    int m_dealer = 0;
    int m_donneNumber = 0;
    int m_dealsGiven = 0;
    int m_donnes = 0;
    int m_redeals = 0;
    int m_matchWinner = -1;

    int m_bidsThisRound = 0;
    Contract m_bid = Contract::Passe;
    int m_bidder = -1;

    int m_taker = -1;
    // À 5 joueurs : le partenaire appelé (-1 tant qu'il n'y en a pas, et
    // quand la carte appelée est au chien ou chez le preneur), la carte
    // appelée et sa couleur.
    int m_callee = -1;
    Suit m_calledSuit = Suit::Pique;
    int m_calledRank = 0;
    bool m_callHidden = false;
    bool m_callRevealed = false;
    Contract m_contract = Contract::Passe;
    bool m_ecartNeeded = false;
    bool m_chelemAnnounced = false;

    int m_trickNumber = 0;
    int m_trickWinner = -1;

    int m_score[kMaxSeats] = {0, 0, 0, 0, 0};
    // Ce que les plis ont rapporté, et les bouts qui y sont tombés. Ces deux
    // comptes-là sont publics : tout le monde a vu les cartes passer, et
    // c'est eux que la colonne de droite affiche pendant la donne.
    int m_points[kTeamCount] = {0, 0};
    int m_bouts[kTeamCount] = {0, 0};
    int m_tricks[kTeamCount] = {0, 0};
    // Et ce que l'écart et le chien y ajouteront à la fin. Tenus à part parce
    // qu'ils ne sont pas publics : afficher le total du preneur pendant la
    // donne dirait à la table ce qu'il a mis de côté.
    int m_extra[kTeamCount] = {0, 0};
    int m_extraBouts[kTeamCount] = {0, 0};
    // Le camp qui a mené le petit au dernier pli, et qui marque donc les dix
    // points. -1 quand le petit est tombé ailleurs, ce qui est le cas presque
    // toujours.
    int m_petitAuBoutTeam = -1;

    DonneResult m_result;
};
