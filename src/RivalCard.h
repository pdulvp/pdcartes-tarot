#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

class QTimer;

// La carte d'un joueur : son médaillon, son nom, son niveau, sa jauge, et ce
// qu'il a fait sous ce profil-là.
//
// Elle vit au menu d'ouverture, là où l'on regarde les six avant de choisir la
// table : un clic invite à la table -- et le premier invité s'assoit en face,
// ce qui en fait votre partenaire -- et un clic droit demande le petit menu.
// La carte ne fait que le demander : c'est la page qui le dresse, parce que
// c'est elle qui sait ce qu'on peut faire d'un joueur.
class RivalCard : public QWidget {
    Q_OBJECT

public:
    explicit RivalCard(const QString &rivalId, QWidget *parent = nullptr);

    QString rivalId() const { return m_rivalId; }

    void setInvited(bool invited);
    bool invited() const { return m_invited; }

    // Au menu, un clic invite. Ailleurs, la carte ne fait que dire.
    void setPickable(bool pickable);

    // Posée sur le drap plutôt que sur le fond pâle d'un dialogue : la carte
    // devient sombre et bordée d'or, et son encre passe à celle du tapis.
    // Elle garde ce qu'elle dit -- sa petite phrase, son palmarès -- faute
    // d'une fiche où le dire ailleurs.
    void setOnFelt(bool onFelt);

    // Le rang dans lequel il a été invité : le premier s'assoit en face de
    // vous, et la carte le dit plutôt que de laisser la surprise pour la
    // première donne.
    void setSeatNote(const QString &note);

    // Après que l'expérience a bougé, ou que le profil a changé.
    void reread();

    QSize sizeHint() const override;

Q_SIGNALS:
    void invitedChanged();
    void menuWanted(const QPoint &where);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    // Le maintien du pointeur sur la carte avant que l'aperçu paraisse : un
    // déplacement à travers les six n'en déclenche aucun.
    QTimer *m_glanceBeat = nullptr;

    QString m_rivalId;
    QString m_seatNote;
    bool m_invited = false;
    bool m_onFelt = false;
    bool m_pickable = false;
    bool m_hovered = false;
};
