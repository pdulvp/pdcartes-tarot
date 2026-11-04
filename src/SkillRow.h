#pragma once

#include <QString>
#include <QWidget>

// Ligne décrivant une compétence d'un adversaire, dans sa fiche ou son aperçu.
//
// La fiche comporte huit lignes de même forme : le nom de la compétence, ses
// pastilles de niveau, l'intitulé du niveau atteint, et l'effet de ce niveau
// en cours de partie. Les acquises comportent en outre leur rang en chiffres
// romains et la jauge de progression vers le rang suivant. Chaque compétence
// a une teinte propre : les quatre teintes chaudes sont le tempérament, les
// quatre teintes froides l'expérience acquise.
//
// Une ligne d'innée est modifiable : ses pastilles sont cliquables, la fiche
// étant seule à connaître les conséquences de cette modification.
//
// En mode compact, la ligne ne conserve que le nom, les pastilles, le rang et
// son intitulé, à hauteur réduite : c'est la forme employée par l'aperçu
// affiché au survol d'un adversaire.
class SkillRow : public QWidget {
    Q_OBJECT

public:
    explicit SkillRow(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setHue(int hue);
    // Niveau atteint, et nombre total de niveaux.
    void setLevel(int level, int outOf);
    void setWord(const QString &word);
    void setSays(const QString &says);
    // Acquises uniquement : le rang en chiffres romains et la jauge de
    // progression vers le rang suivant.
    void setRank(const QString &roman);
    void setGauge(qreal filled, const QString &right);

    // Rend les pastilles cliquables.
    void setEditable(bool editable);
    // Grisée : désigne ce qui sera effacé pendant la modification du
    // tempérament.
    void setDim(bool dim);
    // Mode compact : trois colonnes seulement, à hauteur réduite.
    void setCompact(bool compact);

    int level() const { return m_level; }

    QSize sizeHint() const override;

Q_SIGNALS:
    void levelChosen(int level);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // Pastille située sous ce point, ou -1.
    int pipAt(const QPointF &where) const;
    QRectF pipBox(int index) const;
    qreal titleWide() const;
    qreal romanWide() const;
    qreal wordWide() const;
    qreal rowHigh() const;

    QString m_title;
    QString m_word;
    QString m_says;
    QString m_roman;
    QString m_right;
    int m_hue = 200;
    int m_level = 0;
    int m_outOf = 4;
    qreal m_filled = -1.0;      // négatif : pas de jauge
    bool m_editable = false;
    bool m_dim = false;
    bool m_compact = false;
    int m_hovered = -1;
};
