// Tempérament + quatre acquises (#13) : rivalInnate stable, migration des
// pools, skillFor par rangs, répartition des gains de donne, et parité avec
// les anciens niveaux pour les six partants.
#include "Brain.h"
#include "Profiles.h"
#include "Rivals.h"

#include <QCoreApplication>
#include <QSettings>
#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("pdulvp");
    QCoreApplication::setApplicationName("pdcartes-tarot");
    int failures = 0;

    // 1. rivalInnate : stable, dans les bornes, et les six se ressemblent pas.
    {
        for (const Rival &r : rivals()) {
            const Innates a = rivalInnate(r.id);
            const Innates b = rivalInnate(r.id);
            if (a != b) { std::printf("ECHEC innates instable pour %s\n", qPrintable(r.id)); ++failures; }
            for (int i = 0; i < kInnateCount; ++i) {
                if (a.level[i] < 0 || a.level[i] > kInnateTop) {
                    std::printf("ECHEC niveau innée hors bornes (%s: %d)\n", qPrintable(r.id), a.level[i]);
                    ++failures;
                }
            }
        }
        bool allSame = true;
        const Innates first = rivalInnate(rivals().first().id);
        for (const Rival &r : rivals()) {
            if (rivalInnate(r.id) != first) allSame = false;
        }
        if (allSame) { std::printf("ECHEC les six ont le même tempérament\n"); ++failures; }
        std::printf("innées: stables, bornées, différentes d'un rival à l'autre\n");
        for (const Rival &r : rivals()) {
            const Innates in = rivalInnate(r.id);
            std::printf("  %s: audace=%d prudence=%d vigilance=%d calme=%d (%s)\n", qPrintable(r.id),
                        in.at(Innate::Audace), in.at(Innate::Prudence), in.at(Innate::Vigilance),
                        in.at(Innate::Calme), qPrintable(temperamentLine(in)));
        }
    }

    // 2. poolsFromXp : somme conservée, plafonds respectés, rangs cohérents.
    {
        const Innates neutral;
        Pools p = poolsFromXp(660, neutral);
        if (p.total() != 660) { std::printf("ECHEC poolsFromXp total=%d\n", p.total()); ++failures; }
        for (int i = 0; i < kLearnedCount; ++i)
            if (p.xp[i] < 0 || p.xp[i] > kPoolCeiling) { std::printf("ECHEC pool hors bornes\n"); ++failures; }
        Pools zero = poolsFromXp(0, neutral);
        if (zero.total() != 0) { std::printf("ECHEC poolsFromXp(0)\n"); ++failures; }
        // 660 réparti également -> ~165 par pool -> rang 2 (paliers 0/50/130/220)
        if (rungOf(165) != 2) { std::printf("ECHEC rungOf(165)=%d\n", rungOf(165)); ++failures; }
        if (rungOf(49) != 0 || rungOf(50) != 1 || rungOf(130) != 2 || rungOf(220) != 3) {
            std::printf("ECHEC paliers rungOf\n"); ++failures;
        }
        std::printf("poolsFromXp: total conservé, rangs 0/50/130/220 ok\n");
    }

    // 3. skillFor : rangs -> capacités, innées -> décalage d'un rang max.
    {
        Learning l0;
        l0.xp = 0;
        Innates neutral;   // niveaux 1 -> décalage nul
        Skill s0 = skillFor(l0, neutral);
        if (s0.weighsHand || s0.readsTable || s0.countsCards || s0.cutsSuit || s0.savesPetit ||
            s0.guessesPartner) { std::printf("ECHEC débutant a des capacités\n"); ++failures; }
        if (s0.blunder < 0.29) { std::printf("ECHEC blunder débutant=%.2f\n", s0.blunder); ++failures; }

        Learning l200;
        l200.xp = 200;
        l200.pools = poolsFromXp(200, neutral);   // ~50 par pool -> rang 1
        Skill s200 = skillFor(l200, neutral);
        if (!s200.weighsHand || !s200.readsTable || !s200.cutsSuit || !s200.savesPetit) {
            std::printf("ECHEC intermédiaire sans ses capacités\n"); ++failures;
        }
        if (s200.countsCards || s200.guessesPartner) { std::printf("ECHEC intermédiaire compte les cartes\n"); ++failures; }
        if (s200.blunder > 0.12) { std::printf("ECHEC blunder intermédiaire=%.2f\n", s200.blunder); ++failures; }

        Learning lMax;
        lMax.xp = kMaxExperience;
        for (int i = 0; i < kLearnedCount; ++i) lMax.pools.xp[i] = kPoolCeiling;
        Skill sMax = skillFor(lMax, neutral);
        if (!sMax.weighsHand || !sMax.readsTable || !sMax.countsCards || !sMax.cutsSuit ||
            !sMax.savesPetit || !sMax.guessesPartner || sMax.blunder != 0.0) {
            std::printf("ECHEC expert incomplet\n"); ++failures;
        }

        // Innées : style, jamais un rang. Audace -> hardiesse d'enchère seule.
        Innates bold;
        bold.set(Innate::Audace, 3);
        Skill so = skillFor(l0, bold);   // xp 0 partout
        if (so.weighsHand || so.readsTable || so.countsCards || so.cutsSuit || so.guessesPartner) {
            std::printf("ECHEC audace 3 débloque un rang à xp 0\n"); ++failures;
        }
        if (so.boldness < 1.15) { std::printf("ECHEC boldness audacieux=%.2f\n", so.boldness); ++failures; }
        // Calme -> moins de bourdes, sans toucher aux rangs.
        Innates calm;
        calm.set(Innate::Calme, 3);
        Skill sc = skillFor(l0, calm);
        if (sc.blunder > 0.19 || sc.blunder < 0.17) { std::printf("ECHEC blunder calme=%.2f\n", sc.blunder); ++failures; }
        // Vigilance >= 2 -> devine l'appelé caché même sans mémoire rang 2.
        Learning lv;
        lv.xp = 200;
        lv.pools = poolsFromXp(200, neutral);   // mémoire ~50 -> rang 1
        Innates keen;
        keen.set(Innate::Vigilance, 3);
        Skill sk = skillFor(lv, keen);
        if (!sk.guessesPartner) { std::printf("ECHEC vigilance 3 ne devine pas l'appelé\n"); ++failures; }
        if (sk.countsCards) { std::printf("ECHEC vigilance débloque le compte des cartes\n"); ++failures; }
        std::printf("skillFor: débutant/intermédiaire/expert conformes, innées = style sans rang ok\n");
    }

    // 4. Parité avec les anciens niveaux pour les six partants (migration xp).
    {
        for (const Rival &r : rivals()) {
            Learning l;
            l.xp = r.startXp;
            l.pools = poolsFromXp(r.startXp, rivalInnate(r.id));
            const Skill s = skillFor(l, rivalInnate(r.id));
            const Level level = levelOf(r.startXp);
            std::printf("  %s (xp %d, %s): weighs=%d reads=%d counts=%d cuts=%d petit=%d guess=%d blunder=%.2f bold=%.2f\n",
                        qPrintable(r.id), r.startXp, qPrintable(levelName(level)),
                        s.weighsHand, s.readsTable, s.countsCards, s.cutsSuit, s.savesPetit,
                        s.guessesPartner, s.blunder, s.boldness);
            if (level == Level::Debutant && (s.countsCards || s.weighsHand || s.readsTable)) {
                std::printf("ECHEC %s débutant avec des rangs d'acquis\n", qPrintable(r.id));
                ++failures;
            }
            if (level == Level::Expert && !s.readsTable) {
                std::printf("ECHEC %s expert sans lecture du pli\n", qPrintable(r.id)); ++failures;
            }
        }
    }

    // 5. Persistance : noteRivalDonne répartit le gain entre les pools.
    {
        QSettings s;
        s.remove(QStringLiteral("profil"));
        s.sync();
        noteRivalDonne(QStringLiteral("lulu"), true, true, true);   // 5 + 7 + 5 = 17
        const Learning after = learningFor(QStringLiteral("lulu"));
        const int gained = after.xp - rivalById(QStringLiteral("lulu")).startXp;
        if (gained != 17) { std::printf("ECHEC gain xp=%d (attendu 17)\n", gained); ++failures; }
        if (after.pools.total() != 17) {
            std::printf("ECHEC pools total=%d (attendu 17)\n", after.pools.total()); ++failures;
        }
        // mémoire a dû prendre la plus grosse part (2+3 de 7 unités de deed)
        if (after.pools.at(Learned::Maitrise) < after.pools.at(Learned::Lecture)) {
            std::printf("ECHEC répartition: maitrise=%d < lecture=%d\n",
                        after.pools.at(Learned::Maitrise), after.pools.at(Learned::Lecture));
            ++failures;
        }
        // Relecture : les pools sont persistées.
        const Learning again = learningFor(QStringLiteral("lulu"));
        if (again.pools.total() != 17) { std::printf("ECHEC pools non persistées\n"); ++failures; }
        std::printf("donne notée: xp +%d, pools +%d (persistés), répartition par actions ok\n",
                    gained, after.pools.total());

        // Migration : un profil avec xp mais sans pools se répartit à la lecture.
        QSettings s2;
        s2.remove(QStringLiteral("profil"));
        s2.setValue(profileKey(currentProfile(), QStringLiteral("rival/oriane/xp")), 660);
        s2.sync();
        const Learning mig = learningFor(QStringLiteral("oriane"));
        if (mig.pools.total() != 660) {
            std::printf("ECHEC migration pools=%d\n", mig.pools.total()); ++failures;
        }
        std::printf("migration: oriane xp 660 -> pools %d,%d,%d,%d (total %d)\n",
                    mig.pools.xp[0], mig.pools.xp[1], mig.pools.xp[2], mig.pools.xp[3],
                    mig.pools.total());
    }

    std::printf(failures == 0 ? "TOUT OK\n" : "%d ECHEC\n", failures);
    return failures == 0 ? 0 : 1;
}
