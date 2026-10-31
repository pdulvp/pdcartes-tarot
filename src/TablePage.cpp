#include "TablePage.h"

#include "AskDialog.h"
#include "FeltArt.h"
#include "Palette.h"
#include "Profiles.h"
#include "Rivals.h"
#include "ScoreBoard.h"
#include "TableView.h"
#include "VictoryEffect.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kActionsHeight = 50;

// Combien de temps un adversaire a l'air de réfléchir. Pas pour faire semblant
// d'être lent : un coup qui arrive dans la même milliseconde que le précédent
// est un coup que personne n'a vu passer, et la moitié du tarot est de
// regarder ce que font les trois autres.
constexpr int kThinkMs = 620;

// Et combien de temps le pli reste sur la table avant d'être ramassé. Plus
// long que le reste, parce que c'est le moment où l'on compte : quatre cartes,
// qui les a jouées, et ce que ça fait.
constexpr int kSweepMs = 1150;

// Le chien reste retourné plus longtemps encore : six cartes à lire d'un coup,
// et ce qu'il y a dedans décide de la donne pour les quatre joueurs.
constexpr int kChienMs = 2600;

QPushButton *actionButton(QWidget *parent, const QString &text, const QString &tip,
                          const QString &keys) {
    auto *button = new QPushButton(text, parent);
    button->setToolTip(keys.isEmpty() ? tip : QStringLiteral("%1 (%2)").arg(tip, keys));
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

TablePage::TablePage(QWidget *parent) : QWidget(parent) {
    m_game = new Tarot(this);
    m_view = new TableView(this);
    m_view->watch(m_game);

    auto *rows = new QVBoxLayout(this);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);
    rows->addWidget(m_view, 1);
    rows->addWidget(buildActions());

    // Par-dessus la table, et seulement quand il y a lieu.
    m_board = new ScoreBoard(this);
    m_victory = new VictoryEffect(this);

    m_beat = new QTimer(this);
    m_beat->setSingleShot(true);
    connect(m_beat, &QTimer::timeout, this, &TablePage::playRival);

    m_sweep = new QTimer(this);
    m_sweep->setSingleShot(true);
    connect(m_sweep, &QTimer::timeout, this, &TablePage::sweep);

    m_chienBeat = new QTimer(this);
    m_chienBeat->setSingleShot(true);
    connect(m_chienBeat, &QTimer::timeout, this, &TablePage::takeChien);

    connect(m_game, &Tarot::changed, this, [this] {
        m_view->refresh();
        updateActions();
        Q_EMIT stateChanged();
    });
    connect(m_game, &Tarot::turnBegan, this, &TablePage::onTurnBegan);
    connect(m_game, &Tarot::acted, this, &TablePage::onActed);
    connect(m_game, &Tarot::trickFinished, this, &TablePage::onTrickFinished);
    connect(m_game, &Tarot::donneFinished, this, &TablePage::onDonneFinished);
    connect(m_game, &Tarot::matchFinished, this, &TablePage::onMatchFinished);

    connect(m_view, &TableView::cardPressed, this, &TablePage::onCardPressed);
    connect(m_view, &TableView::cardDoubleClicked, this, &TablePage::onCardDoubleClicked);
    connect(m_view, &TableView::zoomChanged, this, [this](int percent) {
        m_settings.zoom = percent;
        Q_EMIT zoomChanged(percent);
    });

    connect(m_board, &ScoreBoard::goOn, this, [this] {
        m_board->hideBoard();
        m_victory->stop();
        if (m_game->phase() == Phase::MatchOver) {
            startMatch(m_settings);
        } else {
            m_donneCounted = false;
            m_game->startDonne();
        }
        setFocus();
    });
    connect(m_board, &ScoreBoard::leave, this, [this] {
        m_board->hideBoard();
        m_victory->stop();
        Q_EMIT wantsMenu();
    });
    connect(m_victory, &VictoryEffect::finished, this, [this] {
        m_victory->stop();
        m_board->raise();
        m_board->show();
    });

    setFocusPolicy(Qt::StrongFocus);
}

QWidget *TablePage::buildActions() {
    m_actions = new QWidget(this);
    m_actions->setObjectName(QStringLiteral("actionBar"));
    m_actions->setFixedHeight(kActionsHeight);

    m_state = new QLabel(m_actions);
    // Elle absorbe la place qui reste et rend ce qu'il faut quand il n'y en a
    // plus : pendant l'enchère la rangée porte neuf boutons, et c'est la
    // phrase qui doit céder, pas eux.
    m_state->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    m_sortSuit = actionButton(m_actions, tr("Par couleur"),
                              tr("Ranger la main par couleur, les atouts en tête"), tr("T"));
    m_sortValue = actionButton(m_actions, tr("Par valeur"),
                               tr("Ranger la main par valeur, les cartes qui comptent devant"),
                               tr("F"));

    // Les quatre paroles, dans l'ordre où elles se dépassent. Quatre boutons
    // plutôt qu'un bouton et une liste : l'enchère du tarot est un choix entre
    // quatre choses, et elle se fait d'un clic.
    static const Contract kBids[4] = {Contract::Petite, Contract::Garde, Contract::GardeSans,
                                      Contract::GardeContre};
    for (int i = 0; i < 4; ++i) {
        const Contract contract = kBids[i];
        m_bid[i] = actionButton(
            m_actions, contractShort(contract),
            tr("Annoncer %1 — la donne compte ×%2")
                .arg(contractName(contract))
                .arg(contractMultiplier(contract)),
            QString::number(i + 1));
        connect(m_bid[i], &QPushButton::clicked, this, [this, contract] { doBid(contract); });
    }
    m_pass = actionButton(m_actions, tr("Passer"), tr("Laisser parler le suivant"), tr("P"));
    m_chien = actionButton(m_actions, tr("Prendre le chien"),
                           tr("Ramasser les six cartes et faire son écart"), tr("Entrée"));
    m_ecart = actionButton(m_actions, tr("Écarter"),
                           tr("Mettre de côté les six cartes choisies"), tr("Entrée"));
    m_play = actionButton(m_actions, tr("Jouer"), tr("Jouer la carte choisie"), tr("Entrée"));
    m_hint = actionButton(m_actions, tr("Conseil"), tr("Ce que le meilleur joueur ferait"),
                          tr("H"));

    auto *across = new QHBoxLayout(m_actions);
    across->setContentsMargins(14, 0, 14, 0);
    across->setSpacing(7);
    // L'étiquette prend toute la place qui reste plutôt qu'un espace vide :
    // c'est elle qui doit céder quand la rangée se remplit, mais tant qu'il y a
    // de la place elle la prend.
    across->addWidget(m_state, 1);
    across->addWidget(m_sortSuit);
    across->addWidget(m_sortValue);
    across->addSpacing(12);
    for (QPushButton *button : m_bid) across->addWidget(button);
    across->addWidget(m_pass);
    across->addSpacing(12);
    across->addWidget(m_hint);
    across->addWidget(m_chien);
    across->addWidget(m_ecart);
    across->addWidget(m_play);

    connect(m_sortSuit, &QPushButton::clicked, this, [this] { sort(false); });
    connect(m_sortValue, &QPushButton::clicked, this, [this] { sort(true); });
    connect(m_pass, &QPushButton::clicked, this, &TablePage::doPass);
    connect(m_chien, &QPushButton::clicked, this, &TablePage::takeChien);
    connect(m_ecart, &QPushButton::clicked, this, &TablePage::doEcart);
    connect(m_play, &QPushButton::clicked, this, &TablePage::doPlay);
    connect(m_hint, &QPushButton::clicked, this, &TablePage::doHint);

    return m_actions;
}

// ---------------------------------------------------------------------------
// Qui est à table.

QVector<SeatSpec> TablePage::chooseSeats() const {
    SeatSpec you;
    you.name = tr("Vous");
    // Votre couleur est celle du drap : elle appartient au profil et se choisit
    // dans les réglages, la table n'ayant plus de couleur à elle.
    you.hue = currentPalette().hue;

    // Ceux qu'on a demandés d'abord, puis les moins joués : comme ça les six
    // progressent à peu près ensemble au lieu de laisser les trois derniers
    // débutants pour toujours.
    QVector<QString> order;
    for (const QString &id : m_settings.invited) {
        if (order.size() >= kSeatCount - 1) break;
        if (!order.contains(id)) order.append(id);
    }

    QVector<Rival> rest;
    for (const Rival &rival : rivals()) {
        if (!order.contains(rival.id)) rest.append(rival);
    }
    std::stable_sort(rest.begin(), rest.end(), [](const Rival &a, const Rival &b) {
        return learningFor(a.id).donnesPlayed < learningFor(b.id).donnesPlayed;
    });
    for (const Rival &rival : rest) {
        if (order.size() >= kSeatCount - 1) break;
        order.append(rival.id);
    }

    // Le premier invité s'assoit en face de vous. Au tarot à quatre il n'est
    // pas votre partenaire -- les camps se refont à chaque donne -- mais c'est
    // la place qu'on regarde le plus, et c'est donc celle qu'on choisit.
    const QVector<int> places = {2, 1, 3};
    QVector<SeatSpec> round(kSeatCount);
    round[0] = you;
    for (int i = 0; i < order.size() && i < places.size(); ++i) {
        const Rival &rival = rivalById(order.at(i));
        SeatSpec spec;
        spec.rivalId = rival.id;
        // Le nom que ce profil-là lui donne, et non celui avec lequel il est
        // arrivé : c'est celui qu'on lit sur sa plaque tout au long de la
        // partie.
        spec.name = rivalName(rival.id);
        spec.hue = rival.hue;
        round[places.at(i)] = spec;
    }
    return round;
}

void TablePage::startMatch(const GameSettings &settings) {
    m_settings = settings;
    const TableKind &table = tableKindById(settings.tableId);

    m_board->hideBoard();
    m_victory->stop();
    m_beat->stop();
    m_sweep->stop();
    m_chienBeat->stop();
    m_view->stopAnimations();
    m_view->setZoom(settings.zoom);
    m_donneCounted = false;
    m_dealShown = -1;

    const QVector<SeatSpec> seats = chooseSeats();
    m_skills.clear();
    for (const SeatSpec &spec : seats) {
        m_skills.append(spec.rivalId.isEmpty() ? bestSkill()
                                               : skillFor(learningFor(spec.rivalId).xp));
    }

    // La vue regarde la partie avant que la première donne soit distribuée :
    // startMatch() donne tout de suite, et une vue qui ne regarde encore rien
    // n'aurait pas de quoi montrer la distribution.
    m_view->watch(m_game);
    m_game->startMatch(seats, table.donnes, settings.poignees, settings.chelem);
    m_view->clearChosen();
    updateActions();
    setFocus();
}

void TablePage::applySettings(const GameSettings &settings) {
    const QString table = m_settings.tableId;
    const bool poignees = m_settings.poignees;
    const bool chelem = m_settings.chelem;
    m_settings = settings;
    // Ni la table ni les règles ne changent une partie en cours : jeter une
    // donne à moitié jouée parce qu'un réglage a bougé serait un mauvais
    // échange. Elles valent pour la prochaine.
    m_settings.tableId = table;
    m_settings.poignees = poignees;
    m_settings.chelem = chelem;
    m_hint->setVisible(m_settings.offerHints);
    m_view->setZoom(m_settings.zoom);
    // L'ordre de la main, lui, change tout de suite : on l'a changé en
    // regardant sa main, et attendre la donne suivante pour le poser serait
    // répondre à côté.
    sort(m_settings.holdByValue);
    updateActions();
}

void TablePage::refresh() {
    const Palette &scheme = currentPalette();
    // La bande du bas porte les couleurs de celle du haut : ce sont les deux
    // bords de la même fenêtre, et une bande pâle sous un drap vert sombre
    // ressemblait à un panneau posé par une autre application.
    const QColor barFace = feltBarFace(scheme);
    const QColor barInk = scheme.feltText;
    QColor barAside = barInk;
    barAside.setAlphaF(0.62);
    m_actions->setStyleSheet(
        QStringLiteral("QWidget#actionBar { background: %1; border-top: 1px solid %2; }"
                       "QPushButton { background: %3; color: %4; border: 1px solid %2;"
                       " border-radius: 7px; padding: 5px 13px 7px 13px; }"
                       "QPushButton:hover:enabled { border: 1px solid %5; background: %6; }"
                       "QPushButton:pressed:enabled { background: %7; }"
                       "QPushButton:disabled { color: %2; background: %1; border-color: %1; }"
                       "QPushButton[strong=\"true\"]:enabled { background: %5; color: %8;"
                       " border: 1px solid %5; font-weight: bold; }"
                       "QLabel { color: %9; }")
            .arg(barFace.name(), feltGold(scheme, 0.30).name(QColor::HexArgb),
                 feltPlaqueFill(scheme).name(QColor::HexArgb), barInk.name(),
                 scheme.gold.name(), feltPlaqueFill(scheme, 0.14).name(QColor::HexArgb),
                 feltPlaqueFill(scheme, 0.26).name(QColor::HexArgb),
                 readableTextOn(scheme.gold).name(), barAside.name(QColor::HexArgb)));

    QPalette bar = m_actions->palette();
    bar.setColor(QPalette::Window, barFace);
    bar.setColor(QPalette::WindowText, barInk);
    m_actions->setPalette(bar);
    m_actions->setAutoFillBackground(true);

    m_board->refresh();
    m_view->refresh();
    updateActions();
}

int TablePage::mySeat() const {
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) return at;
    }
    return -1;
}

bool TablePage::playing() const {
    return m_game->seatCount() == kSeatCount && m_game->phase() != Phase::Idle;
}

QString TablePage::subtitle() const {
    if (!playing()) return QString();
    const TableKind &table = tableKindById(m_settings.tableId);
    if (m_game->phase() == Phase::MatchOver) {
        const int winner = m_game->matchWinner();
        if (winner < 0) return tr("%1 — partie nulle").arg(table.name);
        const int me = mySeat();
        return winner == me ? tr("%1 — vous gagnez la partie").arg(table.name)
                            : tr("%1 — %2 gagne la partie")
                                  .arg(table.name, m_game->seat(winner).name);
    }
    return tr("%1 — donne %2 sur %3")
        .arg(table.name)
        .arg(std::min(m_game->donneNumber() + 1, table.donnes))
        .arg(table.donnes);
}

int TablePage::thinkingMs(bool first) const {
    const qreal part = m_settings.pace == 0 ? 1.7 : (m_settings.pace == 2 ? 0.42 : 1.0);
    return std::max(40, int((first ? kThinkMs : kThinkMs * 0.7) * part));
}

int TablePage::sweepMs() const {
    const qreal part = m_settings.pace == 0 ? 1.5 : (m_settings.pace == 2 ? 0.50 : 1.0);
    return std::max(120, int(kSweepMs * part));
}

int TablePage::chienMs() const {
    const qreal part = m_settings.pace == 0 ? 1.5 : (m_settings.pace == 2 ? 0.55 : 1.0);
    return std::max(300, int(kChienMs * part));
}

// ---------------------------------------------------------------------------
// Le tour des autres.

void TablePage::onTurnBegan(int seat) {
    m_view->clearChosen();
    showDealIfNew();

    if (m_game->phase() == Phase::Ecart) {
        m_view->clearPicked();
        if (m_game->seat(seat).human()) {
            sayWhatToDo();
            updateActions();
            // Rien à écarter à la garde sans ni à la garde contre : il ne
            // reste que le chelem à proposer, et on passe au jeu.
            if (!m_game->ecartNeeded()) finishEcart();
            return;
        }
        // Le temps que les six cartes du chien finissent d'arriver chez lui :
        // un écart qui part pendant qu'elles volent encore croise six cartes
        // en chemin, et on ne lit plus ni l'un ni l'autre.
        m_beat->start(thinkingMs(true) + chienMs() / 3);
        return;
    }

    if (m_game->phase() == Phase::Playing && m_game->trickNumber() == 1 && !m_poigneeAsked) {
        askPoignee(seat);
    }

    if (m_game->seat(seat).human()) {
        sayWhatToDo();
        updateActions();
        return;
    }
    m_view->say(m_game->phase() == Phase::Bidding
                    ? tr("%1 regarde sa main…").arg(m_game->seat(seat).name)
                    : tr("À %1 de jouer…").arg(m_game->seat(seat).name));
    // Le premier pli d'une donne mérite qu'on attende ; une parole d'enchère
    // et les plis suivants vont plus vite, parce qu'il y a bien moins à
    // regarder.
    m_beat->start(thinkingMs(m_game->phase() == Phase::Playing && m_game->trickNumber() <= 1));
}

void TablePage::onActed(int seat, const QString &tale) {
    Q_UNUSED(seat);
    if (!tale.isEmpty()) m_view->say(tale);
}

// La poignée se montre avant sa première carte, et elle se montre une fois.
void TablePage::askPoignee(int seat) {
    if (!m_game->mayShowPoignee(seat)) return;
    const PoigneeKind kind = m_game->poigneeAvailable(seat);

    if (!m_game->seat(seat).human()) {
        m_game->showPoignee(seat, choosePoignee(*m_game, seat, m_skills.at(seat)));
        return;
    }

    m_poigneeAsked = true;
    const bool show =
        askYesNo(window(), tr("La poignée"),
                 tr("Vous avez de quoi montrer une %1.").arg(poigneeName(kind)),
                 tr("La montrer"),
                 tr("Les %1 atouts sont posés sur la table et vus de tous, puis repris en "
                    "main. Elle vaut %2 points, et ces points-là vont au camp qui gagne la "
                    "donne — le vôtre ou l'autre.")
                     .arg(poigneeTrumps(kind))
                     .arg(pointsText(poigneePoints(kind))));
    m_game->showPoignee(seat, show ? kind : PoigneeKind::Aucune);
}

void TablePage::playRival() {
    const int at = m_game->turn();
    if (at < 0 || at >= m_skills.size()) return;
    if (m_game->seat(at).human()) return;

    if (m_game->phase() == Phase::Bidding) {
        const Contract wanted = chooseBid(*m_game, at, m_skills.at(at));
        if (wanted != Contract::Passe && m_game->makeBid(wanted)) return;
        m_game->pass();
        return;
    }

    if (m_game->phase() == Phase::Ecart) {
        playRivalEcart();
        return;
    }

    if (m_game->phase() != Phase::Playing) return;

    const int card = chooseCard(*m_game, at, m_skills.at(at));
    const Card *held = findCard(m_game->seat(at).hand, card);
    const Card played = held ? *held : Card{};
    if (card >= 0 && m_game->play(card)) {
        m_view->notePlayed(at, played);
        return;
    }

    // Un coup refusé, ce qui veut dire que le cerveau a demandé quelque chose
    // que les règles ne permettent pas. Jamais vu arriver, et c'est
    // précisément pourquoi il y a un filet : une partie bloquée est bien pire
    // qu'un coup mal joué.
    const QVector<int> allowed = m_game->legalCards(at);
    if (allowed.isEmpty()) return;
    const Card *fallback = findCard(m_game->seat(at).hand, allowed.first());
    const Card thrown = fallback ? *fallback : Card{};
    if (m_game->play(allowed.first())) m_view->notePlayed(at, thrown);
}

void TablePage::playRivalEcart() {
    const int at = m_game->turn();
    if (at < 0 || at >= m_skills.size()) return;

    if (m_game->ecartNeeded() && !m_game->ecartDone()) {
        const QVector<int> ecart = chooseEcart(*m_game, at, m_skills.at(at));
        QString why;
        if (!m_game->setEcart(ecart, &why)) {
            // Le filet, comme pour une carte refusée : on prend les six
            // premières cartes que les règles acceptent.
            QVector<int> fallback;
            for (const Card &card : m_game->seat(at).hand) {
                if (fallback.size() >= kChienSize) break;
                if (m_game->mayDiscard(card)) fallback.append(card.id);
            }
            m_game->setEcart(fallback);
        }
        m_view->noteEcart(m_game->ecart().isEmpty() ? QVector<int>() : ecart);
    }
    finishEcart();
}

void TablePage::onTrickFinished(int winner) {
    const int points = m_game->trickValue();
    m_view->say(m_game->seat(winner).human()
                    ? tr("Vous ramassez le pli — %1 points.").arg(pointsText(points))
                    : tr("%1 ramasse le pli — %2 points.")
                          .arg(m_game->seat(winner).name, pointsText(points)));
    m_sweep->start(sweepMs());
}

void TablePage::sweep() {
    if (m_game->phase() != Phase::TrickOver) return;
    const int winner = m_game->trickWinner();
    m_game->sweepTrick();
    m_view->noteTrickTaken(winner);
}

// ---------------------------------------------------------------------------
// Le chien, et l'écart.

void TablePage::takeChien() {
    if (m_game->phase() != Phase::Chien) return;
    const int taker = m_game->taker();
    QVector<int> coming;
    for (const Card &card : m_game->chien()) coming.append(card.id);
    m_game->chienSeen();
    if (taker >= 0) m_view->noteChienTaken(coming);
}

void TablePage::doEcart() {
    const int me = mySeat();
    if (me < 0 || m_game->phase() != Phase::Ecart || m_game->taker() != me) {
        grumble(tr("Ce n'est pas à vous de faire l'écart."));
        return;
    }
    QVector<int> chosen;
    for (int id : m_view->picked()) chosen.append(id);
    QString why;
    if (!m_game->setEcart(chosen, &why)) {
        grumble(why);
        return;
    }
    m_view->noteEcart(chosen);
    finishEcart();
}

void TablePage::finishEcart() {
    const int at = m_game->taker();
    if (at < 0 || m_game->phase() != Phase::Ecart || !m_game->ecartDone()) return;

    if (m_game->mayAnnounceChelem(at)) {
        if (m_game->seat(at).human()) {
            const bool slam =
                askYesNo(window(), tr("Le chelem"), tr("Annoncer le chelem ?"),
                         tr("Annoncer le chelem"),
                         tr("Vous vous engagez aux dix-huit plis. Réussi, il vaut 400 points ; "
                            "manqué, il en coûte 200. L'annonce vous donne l'entame."));
            if (slam) m_game->announceChelem();
        } else if (chooseChelem(*m_game, at, m_skills.at(at))) {
            m_game->announceChelem();
        }
    }
    m_poigneeAsked = false;
    sortAsPreferred();
    m_game->finishEcart();
}

// ---------------------------------------------------------------------------
// La fin d'une donne.

void TablePage::onDonneFinished() {
    m_beat->stop();
    m_sweep->stop();
    m_chienBeat->stop();
    m_view->clearChosen();
    m_view->clearPicked();
    if (m_donneCounted) return;
    m_donneCounted = true;

    const DonneResult &done = m_game->result();
    if (done.played) {
        // Qui a gagné la donne : celui qui a marqué quelque chose. Au tarot
        // c'est exactement le camp qui gagne, le preneur d'un côté et les
        // trois autres de l'autre, et la somme est nulle.
        for (int at = 0; at < m_game->seatCount(); ++at) {
            const Seat &seat = m_game->seat(at);
            const bool won = done.gained[at] > 0;
            if (seat.human()) {
                noteDonnePlayed(won);
                continue;
            }
            noteRivalDonne(seat.rivalId, won, at == done.taker, done.made);
        }

        // Ce qu'ils viennent d'apprendre compte pour la donne suivante, pas
        // pour la prochaine fois qu'on lance le jeu.
        for (int at = 0; at < m_game->seatCount() && at < m_skills.size(); ++at) {
            const Seat &seat = m_game->seat(at);
            if (seat.human()) continue;
            m_skills[at] = skillFor(learningFor(seat.rivalId).xp);
        }
    }

    if (m_game->phase() == Phase::MatchOver) return;   // la feuille suit
    m_board->setGeometry(rect());
    m_board->showDonne(m_game);
    Q_EMIT stateChanged();
}

void TablePage::onMatchFinished() {
    m_beat->stop();
    m_sweep->stop();
    m_chienBeat->stop();
    const int winner = m_game->matchWinner();
    const int me = mySeat();
    const bool youWon = winner >= 0 && winner == me;

    for (int at = 0; at < m_game->seatCount(); ++at) {
        const Seat &seat = m_game->seat(at);
        if (seat.human()) {
            noteMatchPlayed(youWon, m_game->donneNumber());
            continue;
        }
        noteRivalMatch(seat.rivalId, winner >= 0 && at == winner);
    }

    m_board->setGeometry(rect());
    m_victory->setGeometry(rect());

    // La feuille est remplie dans tous les cas. Quand il y a un feu
    // d'artifice elle attend derrière, et elle se montre quand il s'éteint.
    m_board->showMatch(m_game);
    if (youWon && m_settings.celebrate) {
        m_board->hide();
        QVector<Card> cards;
        QVector<QPointF> at;
        m_view->cardsOnScreen(&cards, &at);
        m_victory->start(cards, at, m_view->trickCardHeight() * 0.62, tr("Partie gagnée"),
                         tr("%1 points en %2 donnes")
                             .arg(pointsText(m_game->seatScore(me)))
                             .arg(m_game->donneNumber()));
    }
    Q_EMIT stateChanged();
}

// ---------------------------------------------------------------------------
// Votre tour.

void TablePage::sort(bool byValue) {
    const int me = mySeat();
    if (me < 0) return;
    // Le moteur garde la main dans son ordre ; trier est une affaire d'écran,
    // mais la main est au moteur, alors c'est lui qui la rend triée.
    m_game->sortSeatHand(me, byValue);
    m_view->refresh();
}

void TablePage::sortAsPreferred() {
    if (!m_settings.holdByValue) return;
    // Le moteur rend les mains rangées par couleur, les atouts en tête : c'est
    // l'ordre par défaut, et il n'y a que l'autre à poser.
    sort(true);
}

void TablePage::showDealIfNew() {
    // Une distribution se montre une fois ; onTurnBegan passe ici à chaque
    // parole et à chaque carte jouée. On compte les distributions et non les
    // donnes : une donne annulée fait redonner sans faire avancer la marque,
    // et il y a bien de nouvelles cartes à montrer.
    const int deal = m_game->dealsGiven();
    if (deal == m_dealShown) return;
    if (m_game->phase() != Phase::Bidding) return;
    m_dealShown = deal;
    m_sortedDonne = false;
    m_poigneeAsked = false;
    sortAsPreferred();
    m_view->noteDealt();
}

void TablePage::doBid(Contract contract) {
    QString why;
    if (!m_game->makeBid(contract, &why)) grumble(why);
}

void TablePage::doPass() {
    QString why;
    if (!m_game->pass(&why)) grumble(why);
}

void TablePage::doPlay() {
    const int me = mySeat();
    const int chosen = m_view->chosen();
    if (me < 0 || m_game->turn() != me || m_game->phase() != Phase::Playing) {
        grumble(tr("Ce n'est pas à vous de jouer."));
        return;
    }
    if (chosen < 0) {
        grumble(tr("Choisissez d'abord une carte."));
        return;
    }
    const Card *held = findCard(m_game->seat(me).hand, chosen);
    const Card played = held ? *held : Card{};
    QString why;
    if (!m_game->play(chosen, &why)) {
        grumble(why);
        return;
    }
    m_view->clearChosen();
    m_view->notePlayed(me, played);
}

void TablePage::doHint() {
    const int me = mySeat();
    if (me < 0 || m_game->turn() != me) {
        m_view->say(tr("Ce n'est pas à vous de parler."));
        return;
    }
    m_view->say(advise(*m_game, me));
}

void TablePage::onCardPressed(int cardId) {
    const int me = mySeat();

    // L'écart : un clic retient la carte, un second la remet dans la main. Six
    // cartes sont retenues à la fois, et c'est le bouton qui les envoie.
    if (me >= 0 && m_game->phase() == Phase::Ecart && m_game->taker() == me) {
        const Card *card = findCard(m_game->seat(me).hand, cardId);
        if (!card) return;
        if (!m_view->picked().contains(cardId) && !m_game->mayDiscard(*card)) {
            grumble(isBout(*card)
                        ? tr("%1 est un bout : un bout ne s'écarte pas.").arg(cardName(*card))
                        : (card->suit == Suit::Atout
                               ? tr("On n'écarte un atout que lorsqu'on n'a rien d'autre à "
                                    "jeter.")
                               : tr("Un roi ne s'écarte pas.")));
            return;
        }
        if (!m_view->picked().contains(cardId) && m_view->picked().size() >= kChienSize) {
            grumble(tr("Six cartes sont déjà choisies : reprenez-en une d'abord."));
            return;
        }
        m_view->togglePicked(cardId);
        sayWhatToDo();
        updateActions();
        return;
    }

    if (me < 0 || m_game->turn() != me || m_game->phase() != Phase::Playing) {
        // On peut toujours regarder ses cartes, même quand ce n'est pas à
        // nous : une carte qui se soulève sous le doigt est ce qui fait qu'une
        // main est une main et pas une image.
        m_view->setChosen(cardId);
        return;
    }
    // Un clic choisit, un second sur la même carte la joue. C'est la façon la
    // plus indulgente de jouer à la souris : rien ne part sur un clic de
    // travers, et rien ne demande deux gestes différents.
    if (m_view->chosen() == cardId) {
        doPlay();
        return;
    }
    m_view->setChosen(cardId);
    QString why;
    if (!m_game->legal(me, cardId, &why)) {
        grumble(why);
        return;
    }
    sayWhatToDo();
}

void TablePage::onCardDoubleClicked(int cardId) {
    const int me = mySeat();
    if (me < 0 || m_game->turn() != me || m_game->phase() != Phase::Playing) return;
    m_view->setChosen(cardId);
    doPlay();
}

void TablePage::moveChoice(int step) {
    const int me = mySeat();
    if (me < 0) return;
    const QVector<Card> &hand = m_game->seat(me).hand;
    if (hand.isEmpty()) return;
    int at = 0;
    for (int i = 0; i < hand.size(); ++i) {
        if (hand.at(i).id == m_view->chosen()) at = i;
    }
    if (m_view->chosen() < 0) {
        at = step > 0 ? 0 : hand.size() - 1;
    } else {
        at = (at + step + hand.size()) % hand.size();
    }
    m_view->setChosen(hand.at(at).id);
}

// ---------------------------------------------------------------------------
// Ce que les boutons disent.

void TablePage::updateActions() {
    const int me = mySeat();
    const bool mine = me >= 0 && m_game->turn() == me;
    const bool bidding = mine && m_game->phase() == Phase::Bidding;
    const bool laying = mine && m_game->phase() == Phase::Playing;
    const bool discarding = me >= 0 && m_game->phase() == Phase::Ecart &&
                            m_game->taker() == me && m_game->ecartNeeded();
    const Seat &seat = m_game->seat(std::max(0, me));

    // Un bouton n'est montré que quand le coup qu'il ferait a un sens : les
    // paroles de l'enchère disparaissent dès que quelqu'un a pris, et le
    // bouton Jouer n'existe pas tant qu'il n'y a pas de carte choisie.
    static const Contract kBids[4] = {Contract::Petite, Contract::Garde, Contract::GardeSans,
                                      Contract::GardeContre};
    for (int i = 0; i < 4; ++i) {
        const bool offered = bidding && m_game->mayBid(kBids[i]);
        m_bid[i]->setVisible(bidding);
        m_bid[i]->setEnabled(offered);
    }

    m_pass->setVisible(bidding);
    m_pass->setEnabled(bidding);

    // Le chien reste retourné le temps qu'on le lise. Quand c'est vous qui
    // prenez, rien ne le ramasse tout seul : on regarde six cartes aussi
    // longtemps qu'on veut.
    const bool reading = m_game->phase() == Phase::Chien;
    const bool mineToTake = reading && m_game->taker() == me;
    m_chien->setVisible(mineToTake);
    m_chien->setEnabled(mineToTake);
    if (reading && !mineToTake && !m_chienBeat->isActive()) m_chienBeat->start(chienMs());

    m_ecart->setVisible(discarding);
    m_ecart->setEnabled(discarding && m_view->picked().size() == kChienSize);
    m_ecart->setText(tr("Écarter (%1/%2)").arg(m_view->picked().size()).arg(kChienSize));

    m_play->setVisible(!bidding && !discarding && !mineToTake);
    const int chosen = m_view->chosen();
    m_play->setEnabled(laying && chosen >= 0 && m_game->legal(me, chosen));

    m_sortSuit->setEnabled(me >= 0 && !seat.hand.isEmpty());
    m_sortValue->setEnabled(me >= 0 && !seat.hand.isEmpty());
    m_hint->setEnabled(mine || discarding);
    m_hint->setVisible(m_settings.offerHints);
    m_view->setZoom(m_settings.zoom);

    // Celui qui finit le tour est celui qu'on met en avant.
    m_play->setProperty("strong", m_play->isEnabled());
    m_ecart->setProperty("strong", m_ecart->isEnabled());
    m_chien->setProperty("strong", mineToTake);
    QVector<QPushButton *> dressed = {m_play, m_ecart, m_chien};
    for (QPushButton *button : dressed) {
        button->style()->unpolish(button);
        button->style()->polish(button);
    }

    if (me < 0) {
        setState(QString());
        return;
    }

    if (m_game->phase() == Phase::Bidding) {
        const QString where =
            m_game->bidder() < 0
                ? tr("Enchère — personne n'a parlé")
                : tr("Enchère — %1 par %2")
                      .arg(contractName(m_game->bid()),
                           m_game->seat(m_game->bidder()).human()
                               ? tr("vous")
                               : m_game->seat(m_game->bidder()).name);
        if (!m_settings.offerHints) {
            setState(where);
            return;
        }
        // Ce que la main vaut pour prendre. Savoir qu'elle vaut six points de
        // prise et qu'il en faut six pour une petite est à peu près tout ce
        // qu'il y a à apprendre de l'enchère.
        setState(tr("%1 — main : %2 points de prise")
                     .arg(where, pointsText(handStrength(seat.hand))));
        return;
    }

    if (m_game->taker() < 0) {
        setState(QString());
        return;
    }

    const QString whose = m_game->taker() == me ? tr("vous avez le contrat")
                                                : tr("%1 a le contrat")
                                                      .arg(m_game->seat(m_game->taker()).name);
    if (m_game->phase() == Phase::Chien || m_game->phase() == Phase::Ecart) {
        setState(tr("%1 — %2, ×%3")
                     .arg(contractName(m_game->contract()), whose)
                     .arg(m_game->multiplier()));
        return;
    }
    setState(tr("%1, ×%2 — %3 : %4 points sur %5, %6")
                 .arg(contractName(m_game->contract()))
                 .arg(m_game->multiplier())
                 .arg(whose, pointsText(m_game->teamPoints(kTakerTeam)),
                      pointsText(m_game->takerTarget()),
                      countedText(m_game->takerBouts(), tr("bout"), tr("bouts"))));
}

void TablePage::setState(const QString &text) {
    m_stateText = text;
    elideState();
}

void TablePage::elideState() {
    if (!m_state) return;
    const QFontMetrics metrics(m_state->font());
    m_state->setText(
        metrics.elidedText(m_stateText, Qt::ElideRight, std::max(40, m_state->width())));
}

void TablePage::sayWhatToDo() {
    const int me = mySeat();
    if (me < 0) return;

    if (m_game->phase() == Phase::Ecart && m_game->taker() == me) {
        if (!m_game->ecartNeeded()) {
            m_view->say(tr("Le chien ne se voit pas : la donne se joue telle qu'elle est."));
            return;
        }
        const int left = kChienSize - m_view->picked().size();
        const int forced = m_game->forcedTrumpsInEcart();
        if (left > 0 && forced > 0) {
            m_view->say(tr("Encore %1 à écarter — et il vous faut y mettre %2, faute "
                           "d'autre chose à jeter.")
                            .arg(countedText(left, tr("carte"), tr("cartes")),
                                 countedText(forced, tr("atout"), tr("atouts"))));
        } else if (left > 0) {
            m_view->say(tr("Encore %1 à écarter : ni roi ni bout.")
                            .arg(countedText(left, tr("carte"), tr("cartes"))));
        } else {
            m_view->say(tr("Six cartes de côté : écartez, ou reprenez-en une."));
        }
        return;
    }

    if (m_game->turn() != me) return;

    if (m_game->phase() == Phase::Bidding) {
        if (m_game->bidder() < 0) {
            m_view->say(tr("À vous : prendre à la petite ou mieux, ou passer."));
        } else {
            m_view->say(tr("À vous : annoncer plus que %1, ou passer.")
                            .arg(contractName(m_game->bid())));
        }
        return;
    }
    if (m_game->phase() != Phase::Playing) return;

    const int chosen = m_view->chosen();
    if (chosen >= 0) {
        const Card *card = findCard(m_game->seat(me).hand, chosen);
        if (card) {
            m_view->say(tr("%1 — cliquez encore, ou Jouer.").arg(cardName(*card)));
            return;
        }
    }

    if (!m_game->trickStarted()) {
        m_view->say(tr("À vous d'entamer : vous jouez ce que vous voulez."));
        return;
    }
    const QVector<int> allowed = m_game->legalCards(me);
    if (allowed.size() == 1) {
        const Card *only = findCard(m_game->seat(me).hand, allowed.first());
        m_view->say(tr("Les règles ne vous laissent que %1.")
                        .arg(only ? cardName(*only) : tr("une carte")));
        return;
    }
    if (!m_game->ledKnown()) {
        m_view->say(tr("L'excuse a entamé : c'est à vous de demander une couleur."));
        return;
    }
    const Suit led = m_game->ledSuit();
    const int master = m_game->trickMaster();
    const bool oursHolds = master >= 0 && m_game->teamOf(master) == m_game->teamOf(me);
    if (oursHolds) {
        m_view->say(tr("%1 demandé. Le pli est à votre camp : chargez-le si vous pouvez.")
                        .arg(suitName(led)));
        return;
    }
    m_view->say(tr("%1 demandé, et le pli est à l'autre camp.").arg(suitName(led)));
}

void TablePage::grumble(const QString &why) {
    m_view->sayTrouble(why.isEmpty() ? tr("Ce coup-là n'est pas permis.") : why);
}

// ---------------------------------------------------------------------------

void TablePage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    elideState();
    if (m_board) m_board->setGeometry(rect());
    if (m_victory) m_victory->setGeometry(rect());
}

void TablePage::keyPressEvent(QKeyEvent *event) {
    const int me = mySeat();
    const bool mine = me >= 0 && m_game->turn() == me;
    switch (event->key()) {
    case Qt::Key_T: sort(false); return;
    case Qt::Key_F: sort(true); return;
    case Qt::Key_Left: moveChoice(-1); return;
    case Qt::Key_Right: moveChoice(1); return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (m_game->phase() == Phase::Chien && m_game->taker() == me) {
            takeChien();
        } else if (m_game->phase() == Phase::Ecart && m_game->taker() == me) {
            doEcart();
        } else if (mine && m_game->phase() == Phase::Playing) {
            doPlay();
        }
        return;
    case Qt::Key_P:
        if (mine && m_game->phase() == Phase::Bidding) doPass();
        return;
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_4:
        if (mine && m_game->phase() == Phase::Bidding) {
            static const Contract kBids[4] = {Contract::Petite, Contract::Garde,
                                              Contract::GardeSans, Contract::GardeContre};
            doBid(kBids[event->key() - Qt::Key_1]);
        }
        return;
    case Qt::Key_H:
        if (m_settings.offerHints) doHint();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}
