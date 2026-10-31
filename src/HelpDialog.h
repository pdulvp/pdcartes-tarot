#pragma once

#include <QDialog>

class QTextBrowser;

// Les règles, et comment on joue à la souris.
//
// Le tarot ne se devine pas en cliquant. Les cartes valent des demi-points ;
// trois d'entre elles, les bouts, abaissent le contrat au lieu de rapporter
// davantage ; l'excuse se joue quand on veut et ne prend jamais ; on doit
// fournir, couper quand on ne peut pas fournir, et monter sur un atout posé ;
// le preneur doit ramasser 56, 51, 41 ou 36 points selon le nombre de bouts
// qu'il a dans son tas. Rien de tout cela ne se lit sur le tapis. Alors c'est
// écrit, en entier, dans la fenêtre plutôt que dans un fichier que personne ne
// trouvera.
class HelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);

private:
    QTextBrowser *m_pages = nullptr;
};
