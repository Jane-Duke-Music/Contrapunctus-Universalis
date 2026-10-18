/* ============================================================================
 * Contrapunctus Universalis — définitions partagées
 * ============================================================================
 * Quatre voix aux fréquences continues évoluent sous un ensemble minimal de
 * lois physiques. Le contrepoint n'est pas codé : il émerge des contraintes.
 *
 * Les AXIOMES du système (tout #define ci-dessous est une loi, une condition
 * initiale, ou une limite de perception — jamais une règle musicale) :
 *
 *  A1. Positivité        — aucune fréquence ≤ 0 (physique, non négociable)
 *  A2. Indépendance      — deux voix ne peuvent pas occuper la même fréquence
 *  A3. Répulsion         — deux voix trop proches se repoussent
 *  A4. Bornes de registre— limites de PERCEPTION de l'émission (E1…E7) ;
 *                          les résonances harmoniques, elles, n'ont pas de
 *                          limite de fréquence (voir synth.c).
 *  A5. Bornes temporelles— le temps musical est une grille de TICS, pas des
 *                          secondes. Une note dure entre 1 tic et 960 tics
 *                          (une ronde). L'émission est finie ; l'énergie,
 *                          elle, ne meurt jamais (voir synth.c).
 *
 * Licences : GPL-3.0
 * ========================================================================== */
#ifndef CU_H
#define CU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * A5 — Le réseau de la perception (les tics)
 *
 * 960 tics par ronde. En 4/4, une mesure = une ronde : les deux conventions
 * coïncident. 960 est divisible par 2, 3, 4, 5, 6, 8, 10, 12, 15, 16 — toutes
 * les subdivisions usuelles et la plupart des triolets. (Le standard MIDI
 * compte en tics par noire : 480 TPQN = 1920 par ronde. Nous n'étant pas
 * liés à MIDI, 960/ronde = 240/noire suffit ; une export MIDI v2 fera la
 * conversion ×(1/4).)
 *
 * Tempo : SECONDS_PER_WHOLE = 2.0 s (noire = 120 à la minute) donc 480 tics
 * par seconde. Un tic = 2,083 ms. La borne basse du rythme (1 tic) est donc
 * une note de 2,083 ms — le battement granulaire le plus bref possible.
 * ------------------------------------------------------------------------- */
#define TICKS_PER_WHOLE       960
#define MIN_DUR_TICKS         1
#define MAX_DUR_TICKS         TICKS_PER_WHOLE   /* une ronde : la note la plus longue */
#define SECONDS_PER_WHOLE     2.0
#define TICKS_PER_SECOND      (TICKS_PER_WHOLE / SECONDS_PER_WHOLE)   /* 480 */

/* ---------------------------------------------------------------------------
 * Horloge de la matière (continue, en secondes) vs horloge de la perception
 * (discrète, en tics). La physique ignore la grille ; seules les émissions
 * s'y alignent. huit pas physiques par tic suffisent (dt = 260 µs).
 * ------------------------------------------------------------------------- */
#define SAMPLE_RATE           44100
#define PHYS_SUBSTEPS_PER_TICK 8
#define DT_SECONDS            (SECONDS_PER_WHOLE / (double)(TICKS_PER_WHOLE * PHYS_SUBSTEPS_PER_TICK))
#define SAMPLES_PER_TICK      ((double)SAMPLE_RATE / TICKS_PER_SECOND)  /* 91.875 */

/* ---------------------------------------------------------------------------
 * A4 — Registre commun de l'orchestre à cordes (limites de PERCEPTION de
 * l'émission : la fondamentale émise est bornée ; les résonances ne le sont
 * pas — synth.c rend tous les partiels jusqu'au Nyquist de l'enregistrement).
 * ------------------------------------------------------------------------- */
#define F_MIN                 41.2      /* E1  */
#define F_MAX                 2637.02   /* E7  */
#define F_CENTER              ((F_MIN + F_MAX) / 2.0)   /* 1339.11 Hz — moyenne arithmétique (condition initiale, pas une loi) */

/* ---------------------------------------------------------------------------
 * Les forces. Aucune n'est musicale. L'unité de masse est 1 ; l'accélération
 * est en Hz/s². La dynamique est du second ordre et STRICTEMENT CONSERVATIVE :
 * aucune viscosité, aucun bruit, aucun générateur aléatoire. La vitesse n'est
 * conservée par aucune loi — les voix échangent leur énergie à chaque
 * rencontre, et c'est cet échange chaotique, parfaitement déterministe, qui
 * devient le rythme via la loi d'émission. L'énergie TOTALE, elle, se
 * conserve : le mouvement perpétuel est une conséquence, pas un paramètre.
 * ------------------------------------------------------------------------- */
#define K_REP                 8.0e6   /* répulsion par paire, adoucie de Plummer (voir voices.c) */
#define D_MIN                 4.0     /* Hz — rayon d'adoucissement : barrière anti-unisson (A2) */
#define WALL_MARGIN           200.0   /* Hz — marge intérieure aux bornes       */
#define K_WALL                40.0    /* ressort élastique des bornes (A4)      */

/* ---------------------------------------------------------------------------
 * A5 — La loi rythmique (voir voices.c : voice_onset).
 * La durée d'émission d'une note est une fonction de l'énergie cinétique de
 * la voix comparée à sa propre énergie récente (e_bar). Aucun hasard, aucun
 * LFO : l'irrégularité vient du chaos du système couplé lui-même.
 * ------------------------------------------------------------------------- */
#define RHY_LAMBDA            0.25    /* lissage de l'énergie passée de la voix */
#define RHY_GAIN              1.8     /* sensibilité de la perception           */
#define RHY_EPS               1e-9

/* ---------------------------------------------------------------------------
 * Synthèse — la note éternelle (voir synth.c).
 * Une note émise cesse d'être émise mais ne disparaît pas : queue
 * exponentielle qui ne s'annule mathématiquement jamais. On cesse seulement
 * de la calculer lorsqu'elle passe sous le plancher de PERCEPTION (le
 * quantum du 16 bits). La troncature est perceptuelle, pas physique.
 * ------------------------------------------------------------------------- */
#define ATTACK_SECONDS        0.003   /* anti-clic : un discontinu ferait renvoyer du spectre infini (A1 appliqué à la synthèse) */
#define TAU_RELEASE           0.40    /* s — constante de temps de la queue     */
#define PERCEPTION_FLOOR      (1.0 / 32768.0)  /* un LSB du 16 bits             */
#define MAX_PARTIALS          64      /* au-delà : sous la perception (1/64 ≈ -36 dB) ; la série reste infinie dans l'idéal */
#define WT_SIZE               2048    /* table d'onde sinusoïdale               */

#define NVOICES               4
#define MASTER_PEAK           0.89    /* normalisation finale                   */

typedef enum { WF_SINE = 0, WF_TRIANGLE, WF_SQUARE, WF_SAW, WF_COUNT } waveform_t;

/* Une voix : la particule (f, v) et la note actuellement émise. */
typedef struct {
    double     f;                /* Hz — la particule (la matière)         */
    double     v;                /* Hz/s — jamais conservée                */
    waveform_t wf;               /* masque harmonique attribué à la voix   */

    double     e_bar;            /* énergie cinétique récente lissée (mémoire rythmique) */
    uint64_t   next_onset_tick;  /* prochain tic d'émission                */

    double     note_f;           /* Hz — fondamentale figée à l'émission   */
    uint32_t   dur_ticks;        /* durée d'émission courante, en tics     */
} voice_t;

/* Un événement de note rendu par la synthèse. */
typedef struct {
    int        voice;
    double     f;                /* fondamentale émise (bornée, A4)         */
    waveform_t wf;
    double     amp;              /* amplitude de crête de la note           */
    int64_t    start;            /* échantillon d'émission                  */
    int64_t    sustain_end;      /* fin de l'émission (début de la queue)   */
    int64_t    total;            /* fin de la queue (plancher de perception)*/
} event_t;

typedef struct {
    event_t  *events;
    size_t    n_events, cap_events;
    int64_t   total_samples;
    double   *master;            /* somme de toutes les ondes (la matière) */
} synth_t;

typedef struct world_s {
    voice_t   voices[NVOICES];
    synth_t   syn;
    /* statistiques */
    uint32_t  onset_count[NVOICES];
    uint32_t  dur_min[NVOICES], dur_max[NVOICES];
    uint64_t  dur_sum;  uint64_t onset_total;
    uint32_t  histo[10];
    /* diagnostic : le registre réellement visité par chaque voix dans la
     * fenêtre. Le témoignage ne lit jamais la matière — aucune force ne
     * dépend de ces champs (le monde n'a pas d'observateur).            */
    double    f_seen_min[NVOICES], f_seen_max[NVOICES];
} world_t;

/* voices.c — la matière */
void     voices_init(world_t *w);
void     voices_substep(world_t *w);            /* un pas physique (dt)          */
void     voice_onset(world_t *w, int i, uint64_t tick);  /* A5 : la loi rythmique */

/* synth.c — la note éternelle */
void     synth_init(synth_t *s, int64_t total_samples);
void     synth_add_event(synth_t *s, int voice, double f, waveform_t wf, uint64_t tick, uint32_t dur_ticks);
void     synth_render(synth_t *s);              /* rend tous les événements et normalise */
void     synth_free(synth_t *s);

/* wav.c — l'enregistrement */
int      wav_write(const char *path, const double *samples, int64_t n);

#endif /* CU_H */
