#pragma once

#include <QString>
#include <QWidget>

// Ligne décrivant une compétence d'un adversaire, dans son aperçu.
//
// L'aperçu comporte huit lignes de même forme, réparties en deux colonnes de
// quatre : le nom de la compétence, ses pastilles de niveau, l'intitulé du
// niveau atteint, et pour les acquises le rang en chiffres romains. Chaque
// compétence a une teinte propre, distincte de celle du tapis : les quatre
// teintes chaudes sont le tempérament, les quatre teintes froides
// l'expérience acquise — la couleur seule indique donc la colonne concernée.
//
// En mode compact, la ligne ne conserve que ces colonnes, à hauteur réduite :
// c'est la forme employée par l'aperçu affiché au survol d'un adversaire, qui
// présente huit lignes sur deux colonnes sans place pour une phrase.
class SkillRow : public QWidget {
    Q_OBJECT

public:
    explicit SkillRow(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setHue(int hue);
    // Niveau atteint, et nombre total de niveaux.
    void setLevel(int level, int outOf);
    void setWord(const QString &word);
    // Acquises uniquement : le rang en chiffres romains.
    void setRank(const QString &roman);
    // Mode compact : hauteur réduite, libellé ajusté à la largeur de la ligne.
    void setCompact(bool compact);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF pipBox(int index) const;
    qreal titleWide() const;
    qreal romanWide() const;
    qreal wordWide() const;

    QString m_title;
    QString m_word;
    QString m_roman;
    int m_hue = 200;
    int m_level = 0;
    int m_outOf = 4;
    bool m_compact = false;
};
