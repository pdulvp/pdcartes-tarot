# pdcartes-tarot

Le tarot français, écrit en C++17 avec Qt 6, joué sur un tapis vert à 3, 4 ou 5 joueurs —
vous et trois adversaires qui apprennent d'une partie à l'autre.

Soixante-dix-huit cartes, dix-huit à chacun, six au chien, et un contrat qui
n'est pas un nombre fixe : il baisse à mesure que le preneur ramasse des bouts.
Celui qui prend est seul contre les trois autres, et les camps se refont à
chaque donne — c'est ce qui sépare le tarot de tous les autres jeux de levées.
Six joueurs dont l'expérience est gardée d'une séance à la suivante.

## Jouer

Choisissez une table sur le menu d'ouverture : la partie commence aussitôt. Les
règles sont dans la fenêtre, au bouton **Règles** ou à `F1`.

| | |
|---|---|
| **Petite** **Garde** **Garde sans** **Garde contre** / `1` `2` `3` `4` | annoncer ce contrat ; seules les paroles qui dépassent ce qui est déjà dit sont offertes |
| **Passer** / `P` | laisser parler le suivant |
| **Prendre le chien** / `Entrée` | le ramasser quand on a fini de le lire |
| clic sur une carte, pendant l'écart | la retenir ; un second clic la remet en main |
| **Écarter** / `Entrée` | mettre de côté les six cartes retenues |
| clic sur une carte | la choisir ; elle se soulève |
| second clic, **Jouer**, `Entrée` | la jouer |
| double-clic | la jouer tout de suite |
| `←` `→` | choisir une carte au clavier |
| `T` / `F` | ranger la main par couleur / par valeur |
| `Ctrl` + molette | la taille des cartes, de 80 à 200 % |
| `H` / **Conseil** | ce que le meilleur joueur de la table ferait de votre main |
| `F5` | une autre partie à la même table |
| `Échap` | revenir au menu |

Les cartes que les règles interdisent sont **dessinées éteintes** : il n'y a pas
moyen de jouer un coup défendu, et cliquer sur une carte éteinte dit laquelle
des règles s'y oppose. L'écart suit la même règle — un roi et un bout ne
s'écartent pas, et ils sont éteints pendant qu'on choisit.

## Les règles, en bref

Quatre enseignes de quatorze cartes — de l'as au roi, avec le **cavalier**
glissé entre le valet et la dame —, vingt et un **atouts** numérotés, et
l'**excuse**. L'atout n'est pas une couleur qu'on désigne au début de la
donne : c'est une cinquième famille, imprimée sur les cartes, et elle est la
même à toutes les donnes.

| | |
|---|---|
| Roi | 4,5 point |
| Dame | 3,5 |
| Cavalier | 2,5 |
| Valet | 1,5 |
| Les autres cartes d'enseigne, et les atouts | 0,5 |
| Le petit, le 21, l'excuse | 4,5 |

Quatre-vingt-onze points en tout. À l'intérieur d'une enseigne, le roi passe
devant la dame, qui passe devant le cavalier, puis le valet, puis le dix
jusqu'à l'as : au tarot **l'as ne vaut rien et ne prend rien**, ce qui est
l'inverse de presque tous les autres jeux.

**Les trois bouts** — le petit (l'atout 1), le 21 et l'excuse — ne rapportent
pas davantage que les rois. Ils font autre chose : chacun **abaisse le
contrat**.

| | |
|---|---|
| Aucun bout | il faut 56 points |
| Un bout | 51 |
| Deux bouts | 41 |
| Les trois | 36 |

Prendre le 21 à son adversaire déplace le contrat de dix points d'un coup, ce
qui vaut plus que trois rois. C'est la chose à comprendre du tarot, et c'est à
peu près la seule.

**L'excuse** se joue à tout moment, à la place de n'importe quelle carte, et
sans rien devoir à personne : on peut la poser quand l'atout est demandé et
qu'on en a. En échange, elle ne prend jamais le pli. Elle reste au camp qui l'a
jouée, lequel rend une carte basse au camp qui a pris le pli.

## L'enchère, le chien, l'écart

Dix-huit cartes à chacun, par paquets de trois, et six au chien posées une à
une pendant la distribution. Une main dont le petit est le seul atout et qui
n'a pas l'excuse fait redonner : c'est le **petit sec**.

Chacun à son tour, **une seule fois**, annonce un contrat plus fort que le
précédent ou passe. Si les quatre passent, la donne est annulée.

| | | |
|---|---|---|
| Petite | le chien est retourné et pris par le preneur | ×1 |
| Garde | la même chose | ×2 |
| Garde sans le chien | le chien n'est pas vu, et va au tas du preneur | ×4 |
| Garde contre le chien | le chien n'est pas vu, et va à celui des défenseurs | ×6 |

À la petite et à la garde, le chien est retourné, lu de tous, puis ramassé par
le preneur, qui tient alors vingt-quatre cartes et en met **six de côté**. Ces
six-là comptent dans son tas à la fin, sans que personne les ait vues. On ne
peut écarter ni un roi ni un bout, et les atouts ne s'écartent que lorsqu'il
n'y a rien d'autre à jeter — ils sont alors montrés à tous.

Le but de l'écart n'est pas de mettre des points de côté — ce qu'on a le droit
de jeter ne vaut presque rien — mais de **vider une enseigne** : une couleur
vide, c'est tout le reste de la donne à couper dedans.

## Le jeu

Dix-huit plis, et trois règles :

**Fournir.** On joue l'enseigne demandée si on l'a.

**Couper.** Si on ne l'a pas, il faut jouer atout — toujours, et sans égard pour
qui tient le pli. C'est ce qui distingue le tarot de la belote, où le partenaire
qui tient le pli dispense de couper : ici le preneur est seul, et les trois
défenseurs se doivent l'atout les uns aux autres.

**Monter.** Sur un atout déjà posé, il faut passer dessus si on peut. Si on ne
le peut pas, il faut quand même fournir un atout.

## La marque

On compte ce que le preneur a ramassé — ses plis, plus son écart — et on le
compare à ce qu'il lui fallait selon ses bouts. La différence, l'**écart au
contrat**, décide de tout le reste :

> (25 + écart + petit au bout) × multiplicateur + poignée + chelem

Le résultat est gagné par le preneur si le contrat passe, perdu s'il chute.
Chaque défenseur paie ou reçoit ce montant, et le preneur **trois fois**
celui-ci : la somme des quatre lignes de la feuille de marque est nulle, et
c'est ce qui permet de la vérifier d'un coup d'œil.

**Le petit au bout.** Le petit mené au dernier pli vaut dix points au camp qui
prend ce pli-là, multipliés par le contrat comme le reste. C'est la seule raison
de garder le plus petit atout du jeu jusqu'à la fin — et la seule raison, en
face, de tirer l'atout pour l'en déloger.

**La poignée.** Avant de jouer sa première carte, un joueur qui a beaucoup
d'atout peut les montrer : ils sont posés sur la table, vus de tous, puis repris
en main. Dix atouts valent 20 points, treize 30, quinze 40. L'excuse peut
compléter une poignée, mais seulement si le joueur n'a pas un atout de plus à
montrer à sa place — la montrer, c'est donc dire « je n'ai rien d'autre ».

Et la règle qui surprend : ces points vont au **camp qui gagne la donne**, et
non à celui qui a montré la poignée. Un défenseur qui montre et qui ne fait pas
chuter le contrat vient d'offrir vingt points au preneur, en plus de lui dire où
sont les atouts.

**Le chelem.** Les dix-huit plis. Le preneur peut l'annoncer une fois son écart
fait, et l'annonce lui donne l'entame : annoncé et réussi, 400 points ; annoncé
et manqué, 200 points perdus ; réussi sans avoir été annoncé, 200. La poignée
comme le chelem se laissent de côté : ce sont deux cases dans les réglages.

## Les cinq tables

Le **nombre de joueurs** se choisit sur la page de menu : **3, 4 ou 5**, selon
le règlement de la FFT. À 3 joueurs chacun a 24 cartes (données 4 par 4) et le
chien en a 6 ; à 4, 18 cartes et 6 au chien ; à 5, 15 cartes et 3 au chien, et
le preneur **appelle un roi** après les enchères et avant de voir le chien :
le détenteur de la carte appelée devient son partenaire (l'« appelé ») pour la
donne, sa plaque portant « avec vous » quand c'est vous qui prenez — et une
case des réglages permet de **cacher l'appelé** : personne ne sait alors qui
est le partenaire du preneur tant que la carte appelée n'est pas jouée, et
c'est elle, posée sur un pli, qui révèle le camp (les adversaires ne le
savent pas non plus — seul l'appelé se reconnaît, et les experts le devinent
à qui donne des cartes au preneur) ; si la
carte appelée est au chien ou chez le preneur, il joue seul contre 4. À
l'écart on ne met ni la carte appelée ni sa couleur, et à la première entame
cette couleur est interdite sauf la carte appelée elle-même. La marque se
partage entre attaquants : 2/3 pour le preneur, 1/3 pour l'appelé, chaque
défenseur perd une part. Les contrats sont les mêmes partout, et hors appel
le preneur est seul contre tous les autres. Ce qui change d'une table à l'autre n'est donc pas qui y joue mais
la longueur de la partie, comptée en **donnes**
et non en points : la marque du tarot est individuelle, signée et sans plafond,
et un but en points n'y voudrait rien dire puisque trois joueurs sur quatre sont
ordinairement dans le négatif. Le nombre est un multiple de quatre, pour que
chacun donne autant de fois.

| | | |
|---|---|---|
| Une donne | 1 | une seule, et la meilleure marque l'emporte |
| Le tour de table | 4 | chacun donne une fois |
| Le tapis vert | 8 | le tarot tel qu'on le joue |
| Le cercle | 12 | de quoi se refaire |
| La soirée | 20 | les six y apprennent |

La partie s'arrête au bout du compte, et c'est celui qui mène qui l'emporte. Les
donnes annulées ne comptent pas : quand les quatre passent, on redonne.

## Les six joueurs

Ce ne sont pas six positions d'un même curseur de difficulté. Ce qu'ils savent
est un seul chiffre, leur expérience, qui monte un peu à chaque donne jouée et
un peu plus à chaque donne gagnée, qui ne redescend jamais tout seul, et qui est
encore là au lancement suivant. Trois noms sont posés sur ses plages, et chacun
est une liste de choses qu'un joueur fait ou ne fait pas.

| | expérience | ce qui change |
|---|---|---|
| Débutant | 0 – 199 | compte ses atouts et ses bouts et prend là-dessus, sans voir que quatre rois valent plus que six atouts moyens ; écarte ses six cartes les moins chères, une par couleur, et se retrouve long partout et sans coupe ; joue sa carte sans regarder qui tient le pli ; et se trompe franchement une fois sur trois ou quatre coups |
| Intermédiaire | 200 – 599 | pèse sa main avant de prendre — les bouts, la longueur d'atout, les hauts atouts, les rois, les couleurs courtes ; vide une enseigne à l'écart pour pouvoir y couper ; protège son petit et le pose sur un pli déjà acquis plutôt que de le garder jusqu'au dernier, où il se perd ; charge le pli quand il est à son camp |
| Expert | 600 – 1200 | se souvient des soixante-dix-huit cartes, sait donc combien d'atouts restent dehors et laquelle de ses cartes est devenue maîtresse ; tire l'atout quand il a pris, et se bat pour la main tant qu'il lui en reste à tirer ; laisse filer un pli qui ne vaut rien plutôt que d'y dépenser une maîtresse ; devine l'appelé caché à qui donne des cartes au preneur ; et ne fait plus de bourde |

**L'enchère est réglée sur ce qu'une main rapporte et non sur sa fréquence.** Le
gain moyen du preneur a été mesuré sur quelques milliers de donnes, main par
main : il passe du négatif au positif autour de dix points de prise, et c'est là
que la barre est posée — un peu en dessous, pour qu'une donne sur cinq seulement
soit annulée faute de preneur. La longueur d'atout y pèse bien plus que le
reste, et c'est mesuré aussi : six atouts tiennent le contrat une fois sur huit,
dix une fois sur deux, et les rois ne déplacent cela que de quelques points.

Les barres de la garde, de la garde sans et de la garde contre sont bien plus
hautes, et c'est voulu. La garde ne change rien au jeu — le chien est pris de la
même façon — elle ne fait que doubler ce que la donne rapporte ou coûte. La
garde sans et la garde contre, elles, retirent le chien au preneur tout en
multipliant par quatre et par six. Ce que cela donne à une table de quatre :
21 % de donnes annulées, 83 % de petites, 15 % de gardes, et 47 % de contrats
tenus.

**Trois des six s'assoient** à chaque partie. Au menu, un clic sur une carte
l'invite, et le premier invité s'assoit en face de vous. Les places qui restent
vont aux **moins joués**, pour que les six progressent ensemble au lieu de
laisser les trois derniers débutants pour toujours.

**Trois profils** séparent tout cela : l'expérience des six est comptée dans le
profil sous lequel on joue et nulle part ailleurs, avec son propre compte de
parties gagnées et perdues. Le profil se tient dans la bande du haut, et sa
plaque s'ouvre d'un clic : on en change de là, on le renomme de là, on l'oublie
de là.

Un **clic droit** sur la carte d'un joueur, au menu d'ouverture, le remet au
niveau avec lequel il est arrivé — lui seul, et sur ce profil seulement. Le même
clic droit le **renomme** : il garde sa couleur, son expérience et sa façon de
jouer, et répond au nom qu'on lui donne sur ce profil-là.

## Les cartes

Le tarot dessine ses cartes lui-même par défaut : les atouts portent leur numéro
en chiffres romains, et l'excuse le personnage du joker. Pour en employer
d'autres, on dépose un fichier `.cartes` dans `~/.config/cartes` et on le
choisit dans les réglages, avec son dos. Les deux se jugent **sur l'image** et
non sur un nom : sous le jeu choisi, une main d'exemple montre les quatre
figures, les trois bouts et une carte basse, et les dos s'offrent en vignettes.

Seuls les **tarots** sont proposés : il faut quatorze rangs dans quatre
enseignes, les vingt et un atouts et l'excuse. Un jeu de 52 n'a ni cavaliers ni
atouts, et ne gagnerait pas à être proposé puis refusé. Le dos, lui, a sa propre
liste et n'est pas obligé de venir du jeu des faces : un dos est un dos, et
celui d'un jeu de 52 va aussi bien sur une carte de tarot.

Un jeu installé apporte des images d'une finesse donnée, et une figure réduite
au quart de cela sur le tapis n'est plus qu'un bruit de couleurs. La **taille des
cartes** se règle donc, de 80 à 200 %, dans les réglages ou à `Ctrl` + molette
pendant la partie.

Ces fichiers se fabriquent avec l'atelier, qui est dans `../cartes-set-creator/`
et qui sait aussi ouvrir les `CARDS.DLL` de Windows. Le dossier est commun au
rami et à la belote : un jeu déposé une fois sert aux trois, et chacun n'y
montre que les jeux de son genre.

## La couleur du drap

Six draps au choix — vert, jade, bleu, prune, bordeaux, tabac — et toutes les
couleurs du programme en découlent, la bande du haut et les boîtes de dialogue
comprises. Ce n'est pas la table qui la porte : une partie de vingt donnes n'est
pas d'une autre couleur qu'une partie d'une donne, elle est seulement plus
longue. C'est donc un goût, et un goût appartient à qui joue — **chaque profil
garde le sien**, comme il garde le jeu de cartes, le dos, et le nom qu'il a
donné à Lulu.

## Construire

```sh
cmake -S . -B build
cmake --build build
./build/pdcartes-tarot
```

Un paquet Debian : `cmake --build build --target deb`. L'archive Windows se
construit en croisant la compilation, et les deux paquets d'une version se font
en un pas :

```sh
cmake -S . -B build -DQT_WIN_PREFIX=/chemin/vers/qt6/6.8.2/mingw_64
cmake --build build --target release
```

## Le code

Les règles ne savent pas qu'il y a un écran, et le dessin ne sait pas qu'il y a
des règles. Entre les deux, `TablePage` tient la partie et fait jouer les trois
autres.

| | |
|---|---|
| `src/Cards.cpp` | ce qu'est une carte, et ce qu'elle vaut — en demi-points, comptés en entiers |
| `src/Poignee.cpp` | les atouts qu'une main montre avant de les jouer |
| `src/Tarot.cpp` | la donne : la distribution, le chien, l'enchère, l'écart, les dix-huit plis et la marque |
| `src/Brain.cpp` | comment un joueur décide, et ce qu'un niveau change exactement |
| `src/Rivals.cpp` | les six, leur expérience et les trois niveaux |
| `src/Profiles.cpp` | les trois profils, et où tout cela est gardé |
| `src/Tables.cpp` | les cinq tables, et la longueur d'une partie |
| `src/TableLook.cpp` | la couleur du drap, le jeu et le dos — ce que le profil en cours veut |
| `src/FeltArt.cpp` | le tapis, sa trame, et les cartes posées dessus |
| `src/TableView.cpp` | la table à l'écran : les plaques, le pli en croix, le chien, la marque, votre main, et les cartes qui volent |
| `src/TablePage.cpp` | la page de jeu : les boutons, le rythme des adversaires, le temps que le chien et le pli restent sur la table, et l'expérience comptée en fin de donne |
| `src/ScoreBoard.cpp` | la feuille de marque posée sur le tapis, quatre lignes dont la somme est nulle |
| `src/VictoryEffect.cpp` | les cartes qui s'envolent et le feu d'artifice |
| `src/TablePicker.cpp` | le menu d'ouverture : les tables, les six, et leur petit menu |
| `src/RivalCard.cpp` | la carte d'un joueur, au menu d'ouverture |
| `src/ProfileChip.cpp` | la plaque du profil, dans la bande du haut |
| `src/HelpDialog.cpp` | les règles, en entier, dans la fenêtre |
| `src/MainWindow.cpp` | la fenêtre : la barre du haut et les deux pages |

Le format des jeux de cartes, le dessin d'une carte, la palette et l'habillage
des boîtes de dialogue vivent dans la cartothèque, partagée avec
`cartes-set-creator`, `pdcartes-rami` et `pdcartes-belote`.
