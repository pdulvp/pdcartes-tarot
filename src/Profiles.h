#pragma once

#include <QString>

// Les trois profils.
//
// Une machine, plus d'un joueur -- ou un joueur qui préfère que la série de
// cartes du mois dernier ne compte pas contre celle de ce mois-ci. Un profil
// est le nom sous lequel on joue et, surtout, l'endroit où l'expérience des
// six est gardée : le Lulu qu'on a battu quarante fois sur le premier profil
// n'est pas celui du deuxième, et changer de profil est le seul chemin de
// retour vers une table de débutants.
inline constexpr int kProfileCount = 3;

// Ce qui tient sur la plaque de la bande du haut. Un nom plus long y serait
// coupé, et une plaque dont le nom est coupé ne dit plus lequel des trois
// elle est.
inline constexpr int kProfileNameLength = 24;

struct ProfileRecord {
    int slot = 1;
    QString name;
    int matchesPlayed = 0;
    int matchesWon = 0;
    int donnesPlayed = 0;
    int donnesWon = 0;
    // Le moins de donnes qu'il a fallu pour gagner une partie. C'est le seul
    // record qui veuille dire quelque chose au tarot : le score final est
    // celui de la table qu'on a choisie, et l'avoir atteint en six donnes
    // plutôt qu'en douze est ce qu'on a fait de mieux.
    int bestDonnes = -1;
};

int currentProfile();
void setCurrentProfile(int slot);

QString profileName(int slot);
void setProfileName(int slot, const QString &name);

ProfileRecord profileRecord(int slot);

// Écrit au fur et à mesure plutôt qu'à la fin : une séance qui finit mal
// finit quand même avec les parties qu'elle a jouées comptées.
void noteDonnePlayed(bool won);
void noteMatchPlayed(bool won, int donnes);

// Tout ce que ce profil a : son propre compte et l'expérience des six, remis
// au jour où il était neuf.
void forgetProfile(int slot);

// Où une chose appartenant à un profil est gardée dans le fichier de
// réglages. Un seul endroit qui sait comment la clé s'écrit, pour que la
// lecture et l'écriture ne puissent pas se décaler.
QString profileKey(int slot, const QString &suffix);
