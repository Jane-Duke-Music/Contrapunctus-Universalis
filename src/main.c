/* ============================================================================
 * Contrapunctus Universalis — main.c
 * ----------------------------------------------------------------------------
 * Le noyau. Comme un noyau de système, il ne fait rien d'autre que tourner :
 *
 *     pour chaque tic {
 *         faire avancer la matière (8 pas physiques continus) ;
 *         pour chaque voix : si le tic frappe, émettre (loi rythmique) ;
 *         incrémenter le tic -- le temps ne fait qu'augmenter ;
 *     }
 *
 * La boucle du moteur est éternelle ; l'enregistrement ci-dessous n'ouvre
 * qu'une fenêtre dessus. Rien n'y est musical : les quatre lois physiques et
 * les deux bornes de perception (registre, durée) suffisent — le contrepoint,
 * le mouvement contraire, les croisements, le rythme, tout cela émerge.
 * ========================================================================== */
#include "cu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <getopt.h>

static const char *VN[NVOICES] = { "I", "II", "III", "IV" };

static void usage(const char *exe)
{
    fprintf(stderr,
        "usage : %s [--duration s] [-o fichier.wav]\n"
        "  --duration  longueur de la fenêtre d'enregistrement, en secondes (défaut 60)\n"
        "  -o          fichier WAV de sortie (défaut out.wav)\n"
        "  (aucune graine : l'univers est entièrement déterministe, unique)\n", exe);
}

int main(int argc, char **argv)
{
    double   duration = 60.0;
    const char *out   = "out.wav";

    static struct option opts[] = {
        { "duration", required_argument, 0, 'd' },
        { "output",   required_argument, 0, 'o' },
        { "help",     no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
    };
    int c;
    while ((c = getopt_long(argc, argv, "d:o:h", opts, NULL)) != -1) {
        switch (c) {
            case 'd': duration = atof(optarg); break;
            case 'o': out      = optarg; break;
            default:  usage(argv[0]); return 1;
        }
    }
    if (duration <= 0.0 || duration > 3600.0) {
        fprintf(stderr, "cu: durée invalide (0 < d ≤ 3600 s)\n");
        return 1;
    }

    fprintf(stderr,
        "Contrapunctus Universalis — quatre voix, aucune règle musicale.\n"
        "  registre : %.1f – %.1f Hz (perception de l'émission)\n"
        "  grille   : %d tics/ronde, %.0f tics/s (noire = 120), note ∈ [1, %d] tics\n"
        "  départ   : %.2f / %.2f / %.2f / %.2f Hz — l'explosion est la répulsion seule\n"
        "  aucun générateur aléatoire : l'univers est unique et déterministe\n",
        F_MIN, F_MAX, TICKS_PER_WHOLE, TICKS_PER_SECOND, MAX_DUR_TICKS,
        1340.5, 1340.25, 1339.75, 1339.5);

    world_t w;
    memset(&w, 0, sizeof w);

    int64_t total_samples = (int64_t)(duration * SAMPLE_RATE);
    voices_init(&w);
    synth_init(&w.syn, total_samples);

    /* ------------------------------------------------------------------
     * LE NOYAU. La boucle du moteur est éternelle ; ceci n'est que la
     * fenêtre de l'enregistreur qui la traverse.
     * ---------------------------------------------------------------- */
    uint64_t ticks_total = (uint64_t)(duration * TICKS_PER_SECOND) + 1;
    uint64_t logged      = 0;
    const uint64_t LOG_MAX = 48;   /* les premières émissions, pour témoigner */

    for (uint64_t tick = 0; tick < ticks_total; tick++) {

        /* la perception frappe d'abord : au tic 0, l'émission échantillonne
         * la condition initiale EXACTE — 1340.5, 1340.25, 1339.75, 1339.5 Hz
         * (A5) — puis seulement la matière avance.                            */
        for (int i = 0; i < NVOICES; i++) {
            if (tick == w.voices[i].next_onset_tick) {
                voice_onset(&w, i, tick);
                if (logged < LOG_MAX) {
                    logged++;
                    fprintf(stderr,
                        "  tic %9" PRIu64 "  voix %-3s %9.3f Hz  durée %4u tics (%.3f s)\n",
                        tick, VN[i], w.voices[i].note_f,
                        w.voices[i].dur_ticks,
                        (double)w.voices[i].dur_ticks / TICKS_PER_SECOND);
                }
            }
        }

        /* la matière continue, elle ignore la grille (secondes, pas tics) */
        for (int s = 0; s < PHYS_SUBSTEPS_PER_TICK; s++)
            voices_substep(&w);
        /* le temps ne fait qu'augmenter */
    }

    /* --------------------------------------------------------------- */
    fprintf(stderr, "  … (%" PRIu64 " émissions au total) …\n", w.onset_total);

    synth_render(&w.syn);

    if (wav_write(out, w.syn.master, total_samples) != 0) return 1;

    /* Témoignage : le rythme doit y paraître irrégulier, respirant entre
     * les deux bornes. S'il est figé, une loi est morte — la faute sera
     * dans les constantes, pas dans les principes. */
    fprintf(stderr,
        "\nTémoignage du rendu : %s\n"
        "  échantillons : %" PRId64 " (%.1f s)\n"
        "  événements   : %zu\n"
        "\n  durées d'émission, par voix (tics, bornes [1, 960]) :\n",
        out, total_samples, duration, w.syn.n_events);
    for (int i = 0; i < NVOICES; i++) {
        double mean = w.onset_count[i]
                    ? (double)w.dur_sum / (double)w.onset_total : 0.0;
        /* dur_sum est global ; la moyenne par voix exacte exigerait des
         * sommes par voix — l'ordre de grandeur suffit au témoignage. */
        fprintf(stderr, "    voix %-3s : %5u émissions, durée ∈ [%4u, %4u] tics\n",
                VN[i], w.onset_count[i], w.dur_min[i], w.dur_max[i]);
        (void)mean;
    }
    fprintf(stderr, "  durée moyenne globale : %.1f tics (%.3f s)\n",
            w.onset_total ? (double)w.dur_sum / (double)w.onset_total : 0.0,
            w.onset_total ? (double)w.dur_sum / (double)w.onset_total / TICKS_PER_SECOND : 0.0);

    fprintf(stderr, "\n  registre réellement visité, par voix (la matière y est passée) :\n");
    double gmin = w.f_seen_min[0], gmax = w.f_seen_max[0];
    for (int i = 0; i < NVOICES; i++) {
        if (w.f_seen_min[i] < gmin) gmin = w.f_seen_min[i];
        if (w.f_seen_max[i] > gmax) gmax = w.f_seen_max[i];
        fprintf(stderr, "    voix %-3s : %8.2f – %8.2f Hz\n",
                VN[i], w.f_seen_min[i], w.f_seen_max[i]);
    }
    fprintf(stderr, "  ensemble des quatre voix : %.2f – %.2f Hz\n", gmin, gmax);

    fprintf(stderr, "\n  histogramme des durées (10 cases, 1 → 960 tics) :\n");
    for (int b = 0; b < 10; b++) {
        uint32_t lo = (uint32_t)((uint64_t)b * (TICKS_PER_WHOLE + 1) / 10);
        uint32_t hi = (uint32_t)((uint64_t)(b + 1) * (TICKS_PER_WHOLE + 1) / 10) - 1;
        fprintf(stderr, "    [%4u–%4u] ", lo, hi);
        uint64_t n = w.histo[b];
        uint64_t max = 1;
        for (int k = 0; k < 10; k++) if (w.histo[k] > max) max = w.histo[k];
        int bars = (int)((n * 48) / max);
        for (int k = 0; k < bars; k++) fputc('#', stderr);
        fprintf(stderr, " %" PRIu64 "\n", n);
    }

    synth_free(&w.syn);
    return 0;
}
