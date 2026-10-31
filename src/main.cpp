#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>

#include "CardDeck.h"
#include "Fonts.h"
#include "MainWindow.h"
#include "Version.h"

namespace {

// Qt s'arrête sur le thème d'icônes « hicolor » quand le bureau ne lui a pas
// dit lequel utiliser, et hicolor ne contient presque rien -- ce qui laisse les
// boutons du haut nus. On pointe donc le recours vers un thème d'icônes qui est
// réellement installé.
void ensureIconTheme() {
    const QString configured = QIcon::themeName();
    if (!configured.isEmpty() && configured != QLatin1String("hicolor")) return;

    const QStringList candidates = {QStringLiteral("Adwaita"), QStringLiteral("breeze"),
                                    QStringLiteral("oxygen"), QStringLiteral("Papirus"),
                                    QStringLiteral("gnome")};
    for (const QString &name : candidates) {
        for (const QString &dir : QIcon::themeSearchPaths()) {
            if (!QFileInfo::exists(dir + QLatin1Char('/') + name +
                                   QLatin1String("/index.theme"))) {
                continue;
            }
            QIcon::setFallbackThemeName(name);
            return;
        }
    }
}

// L'icône du jeu, tirée du binaire.
//
// Plusieurs tailles et non une seule qu'on réduirait : chacune est rendue
// depuis le dessin, et une lettre rendue à seize pixels est nette là où la
// même réduite depuis deux cent cinquante-six est molle. C'est la taille de
// la barre des tâches, celle où l'icône se voit le plus.
QIcon builtInIcon() {
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        icon.addFile(QStringLiteral(":/app/pdcartes-tarot-%1.png").arg(size));
    }
    return icon;
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("pdulvp");
    QApplication::setApplicationName("pdcartes-tarot");
    QApplication::setApplicationVersion(fullVersion());
    // Ce sous quoi le bureau connaît cette application. Sous Wayland c'est
    // l'app_id de la fenêtre, et un shell le rapproche du fichier .desktop du
    // même nom pour savoir à quelle application une fenêtre appartient -- ce
    // qui est ce qui permet de l'épingler.
    QApplication::setDesktopFileName(QStringLiteral("pdcartes-tarot"));

    // Le jeu est en français, donc ses nombres et ses dates le sont aussi,
    // quoi que la machine soit réglée.
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    ensureIconTheme();
    // Ce que porte la fenêtre, et ce que le bureau montre dans sa barre.
    // Le thème d'abord, comme pour les icônes des boutons : installé, le jeu
    // a déposé la sienne sous `hicolor`, et un bureau qui habille ses icônes
    // la sert habillée. Le binaire ensuite -- lancé depuis l'arbre de build,
    // et sur Windows où il n'y a pas de thème, c'est le repli qui sert. Après
    // `ensureIconTheme()`, qui vient de dire où chercher.
    QApplication::setWindowIcon(
        QIcon::fromTheme(QStringLiteral("pdcartes-tarot"), builtInIcon()));
    // La fonte de tout ce qui se lit dans la fenêtre. Posée avant la première
    // fenêtre : un widget garde la fonte qu'il avait en naissant.
    dressApplicationFont();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main", "pdcartes-tarot — le tarot français sur tapis vert."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    // Les jeux de cartes installés, lus une fois au démarrage.
    CardDecks::instance().refresh();

    MainWindow window;
    window.show();
    return app.exec();
}
