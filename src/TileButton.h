#pragma once

#include <QAbstractButton>

// Un grand bouton avec une image, un nom et une ligne d'explication.
//
// Le jeu est un jeu : ce qui le lance doit être une chose qu'on montre du
// doigt, pas une ligne dans une liste déroulante. Ce qui est dessiné dans la
// moitié du haut est la table elle-même : quatre places, les deux d'un camp
// dans la couleur de la table et les deux autres de dos, parce qu'au tarot
// la première chose à savoir est qu'on joue avec celui d'en face.
//
// Les cinq tuiles portent toutes le drap du profil : ce qui les distingue est
// la longueur de la partie, et c'est le chiffre de leur coin qui le dit.
class TileButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit TileButton(QWidget *parent = nullptr);

    // La ligne sous le nom, plus petite et plus claire.
    void setBlurb(const QString &blurb);

    // Un chiffre dans le coin: le nombre de points qu'une partie de cette
    // longueur demande, ce genre de chose.
    void setCorner(const QString &corner);

    void setTileSize(const QSize &size);
    // Posée sur le drap plutôt que sur une page pâle : la plaque devient
    // sombre et bordée d'or. Demandé par le menu, et non supposé -- la même
    // tuile sert ailleurs, sur fond clair.
    void setOnFelt(bool onFelt);

    QSize sizeHint() const override;

Q_SIGNALS:
    void pointerIn();
    void pointerOut();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_blurb;
    QString m_corner;
    bool m_onFelt = false;
    QSize m_tileSize = QSize(206, 176);
    bool m_hovered = false;
};
