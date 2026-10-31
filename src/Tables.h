#pragma once

#include <QString>
#include <QVector>

// Les tables où l'on peut s'asseoir, et jusqu'où la partie va.
//
// Cinq tables, et ce qui change d'une à l'autre n'est ni le nombre de joueurs
// -- ce tarot-ci se joue à quatre -- ni les règles, mais la longueur de la
// partie.
//
// La longueur se compte en donnes et non en points, à la différence de la
// belote. La marque du tarot est individuelle, signée, et sans plafond : un
// but en points n'y voudrait rien dire, puisque trois joueurs sur quatre sont
// ordinairement dans le négatif. Une partie de tarot se joue donc sur un
// nombre de donnes fixé d'avance, et ce nombre est un multiple de quatre pour
// que chacun donne autant de fois.
//
// Et la longueur n'est pas un détail : sur quatre donnes une garde contre
// ratée ne se rattrape pas, et il faut prendre ce qui est sûr ; sur vingt, il
// y a le temps de tenter une petite sur une main courte, et le temps aussi
// pour que ce que les six ont appris finisse par se voir.
//
// Elles sont toutes du même drap : la couleur du tapis est un goût, elle se
// choisit dans les réglages et appartient au profil -- voir TableLook.h.
struct TableKind {
    QString id;
    QString name;
    QString blurb;
    int donnes;      // la longueur de la partie, en donnes jouées
};

const QVector<TableKind> &tableKinds();
const TableKind &tableKindById(const QString &id);
QString defaultTableId();
