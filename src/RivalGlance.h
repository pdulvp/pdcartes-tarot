#pragma once

#include <QRect>
#include <QString>
#include <QWidget>

class QTimer;
class QVBoxLayout;

// Aperçu d'un adversaire : ce qu'il est et ce qu'il a appris, affiché pendant
// le survol.
//
// Deux éléments le représentent sans dire où il en est -- sa carte au menu
// d'ouverture et sa plaque au bord du tapis -- et soulèvent la même question :
// celui-là, qu'est-ce qu'il sait ? La page d'aide décrit les trois niveaux,
// mais pas l'adversaire qu'on a sous le pointeur, et il n'y a pas de fiche où
// aller le lire.
//
// L'aperçu y répond sous une forme réduite, sans aucun élément cliquable : son
// médaillon, son nom, son niveau et sa petite phrase d'un côté, son expérience
// et son palmarès de l'autre. Rien de plus : un aperçu doit rester lisible
// d'un coup d'œil.
class RivalGlance : public QWidget {
    Q_OBJECT

public:
    RivalGlance(const QString &rivalId, QWidget *about);

    const QString &rivalId() const { return m_rivalId; }

    // Une fois affiché, l'aperçu surveille lui-même la position du pointeur.
    //
    // Le seul survol de l'élément d'origine ne suffit pas à déterminer sa
    // fermeture : l'aperçu étant affiché par-dessus cet élément -- recouvrement
    // parfois nécessaire en bas d'écran -- il capte le pointeur, l'élément
    // sous-jacent reçoit alors un événement de sortie, et un aperçu fermé sur
    // cet événement se rouvrirait aussitôt, le pointeur étant resté sur
    // l'élément. L'aperçu se ferme donc lorsque le pointeur ne se trouve plus
    // ni sur l'élément d'origine, ni sur lui-même.
    void watch(const QRect &avoid);
    // Indique si le pointeur se trouve encore sur l'un des deux éléments.
    bool wanted() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void buildWhoHeIs(QVBoxLayout *column);
    void buildWhatHeLearned(QVBoxLayout *column);

    QString m_rivalId;
    QRect m_avoid;
    QTimer *m_watch = nullptr;
    int m_hue = 0;
};

// Affiche l'aperçu, unique à l'écran, à la demande du survol.
//
// `avoid` est le rectangle de l'élément survolé, en coordonnées d'écran :
// l'aperçu est placé en dessous, ou au-dessus lorsque le bas de l'écran est
// trop proche. `about` désigne l'élément demandeur, auquel l'aperçu est
// rattaché.
void showRivalGlance(const QString &rivalId, const QRect &avoid, QWidget *about);

// Ferme l'aperçu, à condition qu'il appartienne à l'élément indiqué : deux
// survols successifs ne se ferment donc pas mutuellement. Avec un argument
// nul, l'aperçu est fermé dans tous les cas.
void hideRivalGlance(const QWidget *about = nullptr);

// Libère l'aperçu : il se ferme si le pointeur l'a quitté, et subsiste si le
// pointeur se trouve encore sur lui ou sur l'élément qu'il décrit. Appelée
// lorsque le pointeur sort de la carte ou de la plaque, ce qui se produit
// également lorsque l'aperçu vient de s'afficher par-dessus.
void releaseRivalGlance(const QWidget *about);

// Délai de maintien du pointeur sur un adversaire avant affichage de son
// aperçu.
//
// Il correspond approximativement à celui d'une infobulle : suffisant pour
// qu'un déplacement du pointeur à travers la table ne déclenche pas trois
// aperçus successifs, assez bref pour ne pas être perçu comme une attente.
inline constexpr int kGlanceDelayMs = 420;
