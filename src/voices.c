/* ============================================================================
 * Contrapunctus Universalis — voices.c
 * ----------------------------------------------------------------------------
 * La matière : quatre particules sur l'axe des fréquences, dynamique du
 * second ordre, strictement conservatrice — aucune viscosité, aucun bruit,
 * aucun hasard. La vitesse n'est conservée par aucune loi : les voix
 * échangent leur énergie à chaque rencontre, tantôt elles accélèrent,
 * tantôt elles ralentissent, selon des cycles irréguliers — le chaos
 * déterministe du système couplé. C'est cette respiration, lue au moment
 * de l'émission, qui devient le rythme.
 * ========================================================================== */
#include "cu.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

/* Condition initiale (fondatrice) : les quatre voix précisément à 1340.5,
 * 1340.25, 1339.75 et 1339.5 Hz — répartition symétrique autour de 1340 Hz,
 * au voisinage du centre arithmétique du registre (1339.11 Hz). Aucun
 * hasard : c'est la répulsion de proximité, et elle seule, qui crée
 * l'explosion. Remarque : les voix sont symétriques autour de 1340.0 Hz
 * alors que les bornes du registre le sont autour de 1339.11 Hz — ce
 * décalage infime dissout progressivement la symétrie miroir, sans un
 * seul tirage aléatoire.                                                  */
static const double INIT_F[NVOICES] = { 1340.5, 1340.25, 1339.75, 1339.5 };

static const waveform_t VOICE_WF[NVOICES] = { WF_SINE, WF_TRIANGLE, WF_SQUARE, WF_SAW };

void voices_init(world_t *w)
{
    for (int i = 0; i < NVOICES; i++) {
        voice_t *v = &w->voices[i];
        v->f  = INIT_F[i];                    /* le big-bang attend son heure */
        v->v  = 0.0;                          /* immobile, puis repoussée     */
        v->wf = VOICE_WF[i];
        v->e_bar = 0.0;                       /* aucune mémoire encore        */
        v->next_onset_tick = 0;               /* toutes émettent au tic 0     */
        w->f_seen_min[i] = INIT_F[i];         /* le registre visité commence  */
        w->f_seen_max[i] = INIT_F[i];         /* à la condition initiale      */
        w->onset_count[i] = 0;
        w->dur_min[i] = UINT32_MAX;
        w->dur_max[i] = 0;
    }
    w->dur_sum = 0; w->onset_total = 0;
    memset(w->histo, 0, sizeof w->histo);
}

/* Un pas physique (dt = 1/3840 s). La matière ignore la grille des tics.   */
void voices_substep(world_t *w)
{
    double a[NVOICES];

    /* Répulsion par paire (A2+A3), adoucie à la Plummer :
     *     F = k·d / (d² + ε²)^(3/2)
     * Répulsive PARTOUT — aucun puits parasite à l'unisson —, bornée
     * (maximum à |d| = ε/√2), coulombienne 1/d² au loin. L'énergie paire
     * U = k/√(d²+ε²) culmine à l'unisson : c'est une barrière, pas un piège.
     * Deux voix qui se croisent s'y ralentissent, s'échangent leur élan,
     * puis ré-accélèrent : le battement des rencontres.                   */
    for (int i = 0; i < NVOICES; i++) a[i] = 0.0;
    for (int i = 0; i < NVOICES; i++) {
        for (int j = i + 1; j < NVOICES; j++) {
            double d = w->voices[i].f - w->voices[j].f;
            double F = K_REP * d / pow(d * d + D_MIN * D_MIN, 1.5);
            a[i] += F;
            a[j] -= F;   /* action = réaction */
        }
    }

    for (int i = 0; i < NVOICES; i++) {
        voice_t *v = &w->voices[i];

        /* Bornes de registre (A4) : ressorts dans la marge intérieure. */
        if (v->f < F_MIN + WALL_MARGIN) a[i] += K_WALL * ((F_MIN + WALL_MARGIN) - v->f);
        if (v->f > F_MAX - WALL_MARGIN) a[i] -= K_WALL * (v->f - (F_MAX - WALL_MARGIN));
    }

    /* Intégration semi-implicite d'Euler (symplectique : l'énergie du
     * système ne dérive pas numériquement). */
    for (int i = 0; i < NVOICES; i++) w->voices[i].v += a[i] * DT_SECONDS;
    for (int i = 0; i < NVOICES; i++) w->voices[i].f += w->voices[i].v * DT_SECONDS;

    /* Mur dur aux limites (au-delà du ressort) : rebond parfaitement
     * élastique — le mur ne dissipe rien, l'énergie se conserve.
     * Aucune voix ne sort de la perception ; aucune fréquence ≤ 0 (A1). */
    for (int i = 0; i < NVOICES; i++) {
        voice_t *v = &w->voices[i];
        if (v->f < F_MIN) { v->f = F_MIN; if (v->v < 0.0) v->v = -v->v; }
        if (v->f > F_MAX) { v->f = F_MAX; if (v->v > 0.0) v->v = -v->v; }
    }

    /* Témoignage : le registre réellement visité. Ce n'est pas une force —
     * la matière ne le lit jamais ; seule la fenêtre d'enregistrement
     * regarde. Retirer ces trois lignes ne changerait pas un échantillon. */
    for (int i = 0; i < NVOICES; i++) {
        if (w->voices[i].f < w->f_seen_min[i]) w->f_seen_min[i] = w->voices[i].f;
        if (w->voices[i].f > w->f_seen_max[i]) w->f_seen_max[i] = w->voices[i].f;
    }
}

/* -------------------------------------------------------------------------
 * A5 — La loi rythmique, appelée exactement quand tick == next_onset_tick.
 *
 *   u   = e_bar / (e_bar + Ek)          — la vitesse comparée à sa propre
 *                                         histoire récente (pas de hasard,
 *                                         pas d'oscillateur codé) ;
 *   u2  = 0.5 + GAIN·(u − 0.5)          — la sensibilité de l'oreille ;
 *   dur = 1 + u2·959                    — bornée : [1 tic, 960 tics].
 *
 * Une voix qui accélère articule court ; une voix qui s'apaise soutient.
 * Les durées oscillent irrégulièrement entre les deux bornes parce que
 * l'énergie cinétique elle-même oscille irrégulièrement — répulsion des
 * rencontres, ressorts des bornes, échanges d'élan : rien ne dissipe,
 * rien ne hasarde. Le rythme est la respiration du système conservatif
 * rendue lisible sur la grille des tics.
 * ---------------------------------------------------------------------- */
void voice_onset(world_t *w, int i, uint64_t tick)
{
    voice_t *v = &w->voices[i];

    /* La fondamentale émise est bornée par la perception (A4) ; la particule,
     * elle, continue de vivre dessous — la note est une mesure de la
     * trajectoire, pas la trajectoire. */
    double f = v->f;
    if (f < F_MIN) f = F_MIN;
    if (f > F_MAX) f = F_MAX;

    double Ek = 0.5 * v->v * v->v;
    double u  = v->e_bar / (v->e_bar + Ek + RHY_EPS);
    double u2 = 0.5 + RHY_GAIN * (u - 0.5);
    if (u2 < 0.0) u2 = 0.0;
    if (u2 > 1.0) u2 = 1.0;

    uint32_t dur = MIN_DUR_TICKS + (uint32_t)(u2 * (double)(MAX_DUR_TICKS - MIN_DUR_TICKS));

    v->e_bar = (1.0 - RHY_LAMBDA) * v->e_bar + RHY_LAMBDA * Ek;
    v->note_f = f;
    v->dur_ticks = dur;
    v->next_onset_tick = tick + dur;   /* le temps ne fait qu'augmenter */

    synth_add_event(&w->syn, i, f, v->wf, tick, dur);

    /* statistiques */
    w->onset_count[i]++; w->onset_total++;
    w->dur_sum += dur;
    if (dur < w->dur_min[i]) w->dur_min[i] = dur;
    if (dur > w->dur_max[i]) w->dur_max[i] = dur;
    {
        int b = (int)(((uint64_t)dur * 10) / (TICKS_PER_WHOLE + 1));
        if (b > 9) b = 9;
        w->histo[b]++;
    }
}
