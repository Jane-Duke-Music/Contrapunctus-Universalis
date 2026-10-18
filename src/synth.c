/* ============================================================================
 * Contrapunctus Universalis — synth.c
 * ----------------------------------------------------------------------------
 * La note éternelle.
 *
 * Une note est une onde d'énergie qui réorganise le mouvement de la matière ;
 * l'énergie ne se dissout pas. Quand l'émission cesse, l'onde continue :
 * chaque événement rend ici une queue exponentielle qui ne s'annule
 * mathématiquement JAMAIS. Nous cessons seulement de la calculer quand son
 * amplitude passe sous PERCEPTION_FLOOR — le quantum du 16 bits. La borne est
 * perceptuelle, pas physique : la mortalité des notes est une illusion de
 * notre fenêtre d'enregistrement.
 *
 * Le registre (A4) borne la fondamentale ÉMISE ; les résonances harmoniques,
 * elles, montent sans limite dans l'idéal — nous les rendons toutes jusqu'au
 * Nyquist de l'enregistrement (la véritable limite : la fenêtre d'écoute).
 * La série est donc tronquée par la perception, jamais par une loi musicale.
 * (Le masque d'amplitude 1/n — dents de scie — est la décomposition de
 * Fourier physique de la forme d'onde, pas une échelle.)
 *
 * Aucun repliement de spectre : l'aliasing est précisément le retour des
 * fréquences négatives miroir — interdites par A1. La synthèse additive
 * bornée par Nyquist est A1 appliquée à la synthèse.
 * ========================================================================== */
#include "cu.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Table d'onde sinusoïdale : le seul son primitif, dont tout le reste
 * n'est qu'une somme. */
static double WT[WT_SIZE + 1];
static bool   WT_READY = false;

static void wt_init(void)
{
    if (WT_READY) return;
    for (int i = 0; i <= WT_SIZE; i++)
        WT[i] = sin(6.28318530717958647692 * (double)i / (double)WT_SIZE);
    WT_READY = true;
}

static inline double wt_read(double phase)
{
    phase -= WT_SIZE * floor(phase / (double)WT_SIZE);
    int i = (int)phase;
    double fr = phase - (double)i;
    return WT[i] + fr * (WT[i + 1] - WT[i]);
}

void synth_init(synth_t *s, int64_t total_samples)
{
    s->total_samples = total_samples;
    s->n_events = 0;
    s->cap_events = 256;
    s->events = calloc((size_t)s->cap_events, sizeof(event_t));
    s->master = calloc((size_t)total_samples, sizeof(double));
    if (!s->events || !s->master) {
        fprintf(stderr, "cu: mémoire insuffisante\n");
        exit(1);
    }
    wt_init();
}

void synth_add_event(synth_t *s, int voice, double f, waveform_t wf,
                     uint64_t tick, uint32_t dur_ticks)
{
    if (s->n_events == s->cap_events) {
        s->cap_events *= 2;
        s->events = realloc(s->events, (size_t)s->cap_events * sizeof(event_t));
        if (!s->events) { fprintf(stderr, "cu: mémoire insuffisante\n"); exit(1); }
    }

    /* Correspondance tics -> échantillons, calculée en absolu : la note
     * suivante commence exactement où celle-ci finit (aucune dérive
     * d'arrondi — les deux fronts dérivent du même nombre de tics). */
    double spt = SAMPLES_PER_TICK;
    int64_t start  = llround((double)tick * spt);
    int64_t s_end  = llround((double)(tick + dur_ticks) * spt);

    /* La queue éternelle : τ·ln(A0/floor) — l'instant où l'onde traverse le
     * plancher de perception. En deçà de τ·ln : elle existe ; au-delà : elle
     * existe encore, mais l'enregistrement ne peut plus en témoigner. */
    double amp = 1.0 / (double)NVOICES;
    int64_t tail = (int64_t)((double)TAU_RELEASE * SAMPLE_RATE * log(amp / PERCEPTION_FLOOR));

    event_t *e = &s->events[s->n_events++];
    e->voice       = voice;
    e->f           = f;
    e->wf          = wf;
    e->amp         = amp;
    e->start       = start;
    e->sustain_end = s_end;
    e->total       = s_end + tail;
}

/* Masque harmonique : 1 si le partiel n existe pour la forme d'onde, 0 sinon.
 * a_n = 1/n^p : dents de scie et carrée 1/n (physique), triangle 1/n². */
static inline bool mask_allows(waveform_t wf, uint32_t n) {
    switch (wf) {
        case WF_SAW:      return true;             /* tous                     */
        case WF_SQUARE:   return (n & 1u) == 1u;   /* impairs                  */
        case WF_TRIANGLE: return (n & 1u) == 1u;   /* impairs                  */
        case WF_SINE:     return n == 1u;          /* le fondamental seul      */
        default:          return false;
    }
}
static inline double mask_power(waveform_t wf) {
    return (wf == WF_TRIANGLE) ? 2.0 : 1.0;
}

static void render_event(synth_t *s, const event_t *e)
{
    int64_t len = e->total;
    if (len > s->total_samples - e->start) len = s->total_samples - e->start;
    if (len <= 0) return;

    /* La fondamentale émise est bornée (A4) ; ses résonances montent
     * librement vers Nyquist — la seule borne : la fenêtre d'écoute. */
    int nmax = (int)((0.5 * SAMPLE_RATE) / e->f);
    if (nmax < 1) nmax = 1;
    if (nmax > MAX_PARTIALS) nmax = MAX_PARTIALS;

    /* amplitudes 1/n^p, normalisées en énergie (masque physique de Fourier) */
    double a[MAX_PARTIALS];
    double om[MAX_PARTIALS];
    double ph[MAX_PARTIALS];
    int    np = 0;
    double p = mask_power(e->wf);
    double norm = 0.0;
    for (int n = 1; n <= nmax; n++) {
        if (!mask_allows(e->wf, (uint32_t)n)) continue;
        double m = 1.0 / pow((double)n, p);
        a[np] = m; norm += m * m;
        om[np] = 6.28318530717958647692 * e->f * (double)n / (double)SAMPLE_RATE;
        ph[np] = 0.0;   /* toute émission commence à la phase nulle */
        np++;
    }
    norm = sqrt(norm);
    for (int k = 0; k < np; k++) a[k] = e->amp * a[k] / norm;

    /* enveloppe : attaque (anti-clic = anti-spectre-infini), émission pleine,
     * puis la queue qui ne finit pas. */
    int64_t atk = (int64_t)(ATTACK_SECONDS * SAMPLE_RATE);
    int64_t sustain = e->sustain_end - e->start;
    if (atk > sustain / 3) atk = sustain / 3;
    if (atk < 1) atk = 1;

    double tau_s = TAU_RELEASE * (double)SAMPLE_RATE;
    double *out = s->master + e->start;

    for (int64_t i = 0; i < len; i++) {
        double env;
        if (i < atk)              env = (double)(i + 1) / (double)atk;
        else if (i < sustain)     env = 1.0;
        else                      env = exp(-(double)(i - sustain) / tau_s);
        double sum = 0.0;
        for (int k = 0; k < np; k++) {
            sum += a[k] * wt_read(ph[k]);
            ph[k] += om[k];
        }
        out[i] += env * sum;
    }
}

/* Le fond sonore est la somme des quatre voix seule : aucune nappe de bruit,
 * aucun générateur aléatoire — la matière n'est faite que d'ondes émises. */
void synth_render(synth_t *s)
{
    for (size_t k = 0; k < s->n_events; k++)
        render_event(s, &s->events[k]);

    /* normalisation : mesurer le sommet, ramener à MASTER_PEAK */
    double peak = 0.0;
    for (int64_t n = 0; n < s->total_samples; n++) {
        double x = fabs(s->master[n]);
        if (x > peak) peak = x;
    }
    if (peak > 0.0) {
        double g = MASTER_PEAK / peak;
        for (int64_t n = 0; n < s->total_samples; n++) s->master[n] *= g;
    }
}

void synth_free(synth_t *s)
{
    free(s->events);   s->events = NULL;
    free(s->master);   s->master = NULL;
}
