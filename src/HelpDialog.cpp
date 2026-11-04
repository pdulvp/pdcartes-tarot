#include "HelpDialog.h"

#include "DialogChrome.h"
#include "Palette.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

// La feuille de style du document. Les couleurs viennent de la table que le
// jeu porte, comme partout ailleurs : une boîte de règles ouverte sur une
// table bordeaux est bordeaux.
QString sheetFor(const Palette &scheme) {
    return QStringLiteral(R"CSS(
        h1 { color: %1; font-size: 17px; margin-top: 22px; margin-bottom: 2px; }
        h2 { color: %1; font-size: 13px; margin-top: 16px; margin-bottom: 2px; }
        p { color: %2; margin-top: 6px; margin-bottom: 6px; }
        p.lead { color: %2; font-size: 13px; }
        li { color: %2; margin-bottom: 3px; }
        b { color: %1; }
        th { color: %3; text-align: left; padding-right: 14px;
             padding-top: 3px; padding-bottom: 3px; font-weight: bold; }
        td { color: %2; padding-top: 3px; padding-bottom: 3px; }
        kbd { color: %1; font-weight: bold; }
    )CSS")
        .arg(scheme.accent.name(), scheme.surfaceText.name(), scheme.hint.name());
}

// Les règles, en entier. Écrites ici plutôt que dans un fichier à côté du
// binaire : un fichier à côté du binaire est un fichier qui manque sur la
// machine de quelqu'un d'autre.
const char *kRules = R"HTML(
<h1>Le tarot français</h1>

<p class="lead">Le tarot se joue ici à 3, 4 ou 5 joueurs, avec soixante-dix-huit cartes.
Un joueur prend la donne et se retrouve <b>seul contre les trois autres</b>
pour cette donne-là : les camps sont refaits à chaque fois, et personne ne
choisit le sien. Le preneur doit ramasser un certain nombre de points ; les
trois autres essaient de l'en empêcher.</p>

<h2>Les cartes</h2>

<p>Quatre enseignes de quatorze cartes — de l'as au roi, avec le
<b>cavalier</b> glissé entre le valet et la dame —, vingt et un <b>atouts</b>
numérotés de 1 à 21, et l'<b>excuse</b>. L'atout n'est pas une enseigne qu'on
désigne au début de la donne : c'est une cinquième famille, imprimée sur les
cartes, et elle est la même à toutes les donnes.</p>

<table>
<tr><th>Roi</th><td>4,5 point</td></tr>
<tr><th>Dame</th><td>3,5</td></tr>
<tr><th>Cavalier</th><td>2,5</td></tr>
<tr><th>Valet</th><td>1,5</td></tr>
<tr><th>Les autres cartes d'enseigne, et les atouts</th><td>0,5</td></tr>
<tr><th>Le petit, le 21, l'excuse</th><td>4,5</td></tr>
</table>

<p>Quatre-vingt-onze points en tout. À l'intérieur d'une enseigne, le roi passe
devant la dame, qui passe devant le cavalier, puis le valet, puis le dix
jusqu'à l'as : au tarot l'<b>as ne vaut rien et ne prend rien</b>, ce qui est
l'inverse de presque tous les autres jeux. Les atouts se classent par leur
numéro, et n'importe quel atout passe devant n'importe quelle carte
d'enseigne.</p>

<h2>Les trois bouts</h2>

<p>Le <b>petit</b> (l'atout 1), le <b>21</b> et l'<b>excuse</b>. Ce sont les
trois cartes qui décident de la donne, et elles le font d'une façon
particulière : elles ne rapportent pas davantage que les rois, mais chacune
<b>abaisse le contrat</b> du preneur.</p>

<table>
<tr><th>Aucun bout</th><td>il faut 56 points</td></tr>
<tr><th>Un bout</th><td>51</td></tr>
<tr><th>Deux bouts</th><td>41</td></tr>
<tr><th>Les trois</th><td>36</td></tr>
</table>

<p>Ce n'est pas un détail : prendre le 21 à son adversaire déplace le contrat
de dix points d'un coup, ce qui vaut plus que trois rois.</p>

<h2>L'excuse</h2>

<p>Elle se joue <b>à tout moment</b>, à la place de n'importe quelle carte, et
sans rien devoir à personne : on peut la poser quand l'atout est demandé et
qu'on en a. En échange, elle <b>ne prend jamais le pli</b>. Elle reste au camp
qui l'a jouée, lequel rend une carte basse au camp qui a pris le pli. Jouée au
dernier pli, elle s'en va avec le pli — sauf si son camp a pris tous les
autres.</p>

<h1>La donne et l'enchère</h1>

<p>Dix-huit cartes à chacun, par paquets de trois, et <b>six au chien</b> posées
une à une pendant la distribution. Une main dont le petit est le seul atout et
qui n'a pas l'excuse fait redonner : c'est le <b>petit sec</b>, et son
détenteur perdrait ce bout au premier tour d'atout sans y pouvoir rien.</p>

<p>Chacun à son tour, une seule fois, annonce un contrat plus fort que le
précédent ou passe. Si les quatre passent, la donne est annulée.</p>

<table>
<tr><th>Petite</th><td>le chien est retourné et pris par le preneur ; la donne
compte <b>×1</b></td></tr>
<tr><th>Garde</th><td>la même chose, et la donne compte <b>×2</b></td></tr>
<tr><th>Garde sans le chien</th><td>le chien n'est pas vu et va au tas du
preneur ; <b>×4</b></td></tr>
<tr><th>Garde contre le chien</th><td>le chien n'est pas vu et va au tas des
défenseurs ; <b>×6</b></td></tr>
</table>

<h2>L'écart</h2>

<p>À la petite et à la garde, le chien est retourné, lu de tous, puis ramassé
par le preneur, qui tient alors vingt-quatre cartes et en met <b>six de
côté</b>. Ces six-là comptent dans son tas à la fin, sans que personne les ait
vues.</p>

<p>On ne peut écarter ni un <b>roi</b> ni un <b>bout</b>. Les atouts ne
s'écartent que lorsqu'il n'y a rien d'autre à jeter, et ils sont alors montrés
à tous. Le but n'est pas de mettre des points de côté — ce qu'on a le droit de
jeter ne vaut presque rien — mais de <b>vider une enseigne</b> : une couleur
vide, c'est tout le reste de la donne à couper dedans.</p>

<h1>Le jeu</h1>

<p>Dix-huit plis. Trois règles, et elles sont tout le tarot :</p>

<ul>
<li><b>Fournir</b> : on joue l'enseigne demandée si on l'a.</li>
<li><b>Couper</b> : si on ne l'a pas, il faut jouer atout — toujours, et sans
égard pour qui tient le pli. C'est ce qui distingue le tarot de la belote, où
le partenaire qui tient le pli dispense de couper.</li>
<li><b>Monter</b> : sur un atout déjà posé, il faut passer dessus si on peut.
Si on ne le peut pas, il faut quand même fournir un atout.</li>
</ul>

<p>Les cartes que les règles interdisent sont <b>dessinées éteintes</b> : il
n'y a pas moyen de jouer un coup défendu, et cliquer sur une carte éteinte dit
laquelle des règles s'y oppose.</p>

<h1>La marque</h1>

<p>À la fin de la donne, on compte ce que le preneur a ramassé — ses plis, plus
son écart — et on le compare à ce qu'il lui fallait selon ses bouts. La
différence, l'<b>écart au contrat</b>, décide de tout le reste :</p>

<p><b>(25 + écart + petit au bout) × multiplicateur + poignée + chelem</b></p>

<p>Le résultat est gagné par le preneur si le contrat passe, perdu s'il chute.
Chaque défenseur paie ou reçoit ce montant, et le preneur <b>trois fois</b>
celui-ci : la somme des quatre lignes de la feuille est nulle.</p>

<h2>Le petit au bout</h2>

<p>Le petit mené au <b>dernier pli</b> vaut dix points au camp qui prend ce
pli-là, et ces dix points sont multipliés par le contrat comme le reste. C'est
la seule raison de garder le plus petit atout du jeu jusqu'à la fin — et la
seule raison, en face, de tirer l'atout pour l'en déloger.</p>

<h2>La poignée</h2>

<p>Avant de jouer sa première carte, un joueur qui a beaucoup d'atout peut les
<b>montrer</b> : ils sont posés sur la table, vus de tous, puis repris en main.
L'excuse peut compléter une poignée, mais seulement si le joueur n'a pas un
atout de plus à montrer à sa place.</p>

<table>
<tr><th>Simple poignée</th><td>10 atouts — 20 points</td></tr>
<tr><th>Double poignée</th><td>13 atouts — 30 points</td></tr>
<tr><th>Triple poignée</th><td>15 atouts — 40 points</td></tr>
</table>

<p>Et la règle qui surprend : ces points vont au <b>camp qui gagne la donne</b>,
et non à celui qui a montré la poignée. Un défenseur qui montre et qui ne fait
pas chuter le contrat vient d'offrir vingt points au preneur, en plus de lui
dire où sont les atouts.</p>

<h2>Le chelem</h2>

<p>Les dix-huit plis. Le preneur peut l'<b>annoncer</b> une fois son écart
fait, et l'annonce lui donne l'entame. Annoncé et réussi, il vaut 400 points ;
annoncé et manqué, il en coûte 200. Réussi sans avoir été annoncé, il en vaut
200. On peut jouer sans : c'est une case dans les réglages, comme les
poignées.</p>

<h1>Jouer à la souris</h1>

<table>
<tr><th>Petite, Garde, Garde sans, Garde contre</th><td>annoncer ce
contrat-là ; seules les paroles qui dépassent ce qui est dit sont
offertes</td></tr>
<tr><th>Passer</th><td>laisser parler le suivant</td></tr>
<tr><th>Prendre le chien</th><td>le ramasser quand on a fini de le lire</td></tr>
<tr><th>clic sur une carte, pendant l'écart</th><td>la retenir ; un second clic
la remet en main</td></tr>
<tr><th>Écarter</th><td>mettre de côté les six cartes retenues</td></tr>
<tr><th>clic sur une carte</th><td>la choisir ; elle se soulève</td></tr>
<tr><th>second clic, Jouer, <kbd>Entrée</kbd></th><td>la jouer</td></tr>
<tr><th>double-clic</th><td>la jouer tout de suite</td></tr>
<tr><th><kbd>←</kbd> <kbd>→</kbd></th><td>choisir une carte au clavier</td></tr>
<tr><th><kbd>1</kbd> <kbd>2</kbd> <kbd>3</kbd> <kbd>4</kbd> / <kbd>P</kbd></th>
<td>les quatre contrats, et passer</td></tr>
<tr><th><kbd>T</kbd> / <kbd>F</kbd></th><td>ranger la main par couleur / par
valeur</td></tr>
<tr><th><kbd>Ctrl</kbd> + molette</th><td>la taille des cartes, de 80 à
200 %</td></tr>
<tr><th><kbd>H</kbd> / Conseil</th><td>ce que le meilleur joueur de la table
ferait de votre main</td></tr>
<tr><th><kbd>F5</kbd></th><td>une autre partie à la même table</td></tr>
<tr><th><kbd>Échap</kbd></th><td>revenir au menu</td></tr>
</table>

<h2>Les cinq tables</h2>

<p>Ce qui change d'une table à l'autre est la <b>longueur de la partie</b>,
comptée en donnes : une, quatre, huit, douze ou vingt. Et le <b>nombre de
joueurs</b> se choisit aussi sur la page de menu : <b>3, 4 ou 5</b>, selon le
règlement de la FFT. À 3 joueurs chacun a 24 cartes (données 4 par 4) et le
chien en a 6 ; à 4, 18 cartes et 6 au chien ; à 5, 15 cartes et 3 au chien.
Les contrats sont les mêmes, et le preneur est seul contre tous les autres —
sauf à 5 joueurs : après les enchères et <b>avant de voir le chien</b>, le
preneur <b>appelle un roi</b> de son choix (une dame s'il a les quatre rois,
puis un cavalier, puis un valet), et le détenteur de cette carte devient son
partenaire, l'<b>appelé</b>, pour la donne — sa plaque porte « avec vous »
quand c'est vous qui prenez, une fois la carte appelée jouée. L'appelé reste
<b>caché</b> : personne ne sait
qui est le partenaire du preneur tant que la carte appelée n'a pas été posée
sur un pli — c'est elle, publique, qui révèle le camp. Si la carte appelée est au chien ou chez le
preneur, il joue seul contre les quatre autres. À l'écart, on ne met ni la
carte appelée ni sa couleur ; à la première entame, cette couleur est
interdite sauf si c'est la carte appelée elle-même. La marque se partage alors
entre les deux attaquants : 2/3 pour le preneur, 1/3 pour l'appelé, et chaque
défenseur perd une part.
La longueur n'est pas un détail — sur quatre donnes une garde contre ratée ne
se rattrape pas ; sur vingt, il y a le temps de tenter une petite sur une main
courte, et le temps aussi pour que ce que les six adversaires ont appris
finisse par se voir.</p>

<h2>Les six joueurs</h2>

<p>Trois des six s'assoient à chaque partie. Ce qu'ils savent est un seul
chiffre, leur expérience, qui monte un peu à chaque donne jouée et un peu plus
à chaque donne gagnée, qui ne redescend jamais tout seul, et qui est encore là
au lancement suivant. Un <b>débutant</b> compte ses atouts et prend là-dessus,
écarte ses six cartes les moins chères et joue sans regarder qui tient le pli ;
un <b>intermédiaire</b> pèse sa main, écarte pour se faire une coupe et protège
son petit ; un <b>expert</b> se souvient des soixante-dix-huit cartes, sait
combien d'atouts restent dehors, tire l'atout quand il a pris, chasse le
petit de l'adversaire, et — à 5 joueurs — devine l'appelé caché, à qui donne
des cartes aux plis du preneur. Ni eux ni vous ne connaissez l'appelé avant
que la carte appelée soit jouée, sauf l'appelé lui-même qui la tient.
Chacun a aussi un <b>tempérament</b> — audace, prudence, vigilance, calme —
qui ne le rend ni meilleur ni moins bon, mais reconnaissable, et ce qu'il a
appris se lit acquise par acquise — lecture, mémoire, défausse, maîtrise —
dans l'aperçu de sa carte. L'expérience des six est comptée dans le <b>profil</b>
sous lequel on joue et nulle part ailleurs.</p>

<h2>Les cartes et le drap</h2>

<p>Les cartes que le jeu dessine lui-même ne sont pas les seules : les réglages
listent les tarots posés dans le dossier des cartes, et l'atelier sait en
fabriquer — il ouvre aussi les <b>CARDS.DLL</b> de Windows. Sous le jeu choisi,
une main d'exemple montre les quatre figures, les trois bouts et une carte
basse, et les dos s'offrent en vignettes sur leur propre page : un jeu de
cartes se juge sur l'image et non sur son nom. Cette page-là a sa propre liste
de jeux, si bien que le dos n'est pas obligé de venir de celui des faces — par
défaut il le suit, ce qui est le cas ordinaire. La couleur du drap se choisit
là aussi. Les trois — le jeu, le dos, le drap — appartiennent au profil :
changer de profil est changer de table, et une table n'a pas forcément le même
tapis que l'autre.</p>
)HTML";

} // namespace

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Les règles du tarot"));

    const DialogChrome chrome = applyDialogChrome(this, tr("Les règles"));
    const Palette &scheme = currentPalette();

    m_pages = new QTextBrowser(this);
    m_pages->setOpenExternalLinks(false);
    m_pages->document()->setDefaultStyleSheet(sheetFor(scheme));
    m_pages->setHtml(QString::fromUtf8(kRules));
    m_pages->moveCursor(QTextCursor::Start);
    chrome.fields->addWidget(m_pages, 1);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *ok = buttons->addButton(tr("Entendu"), QDialogButtonBox::AcceptRole);
    markDefaultButton(ok);
    ok->setDefault(true);
    chrome.footer->addStretch();
    chrome.footer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    resize(680, 740);
}
