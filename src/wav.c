/* ============================================================================
 * Contrapunctus Universalis — wav.c
 * ----------------------------------------------------------------------------
 * L'enregistrement : WAV 44,1 kHz / 16 bits / mono, sans dépendance externe.
 * Mono, parce que l'espace des voix est un axe : une dimension, quatre
 * points. Le format conteneur n'est qu'une fenêtre — le contrepoint, lui,
 * continue de toute éternité de l'autre côté du dernier échantillon.
 * ========================================================================== */
#include "cu.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

static void put_u32(FILE *fp, uint32_t x) {
    unsigned char b[4] = { x & 0xFF, (x >> 8) & 0xFF, (x >> 16) & 0xFF, (x >> 24) & 0xFF };
    fwrite(b, 1, 4, fp);
}
static void put_u16(FILE *fp, uint16_t x) {
    unsigned char b[2] = { x & 0xFF, (x >> 8) & 0xFF };
    fwrite(b, 1, 2, fp);
}

int wav_write(const char *path, const double *samples, int64_t n)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) { fprintf(stderr, "cu: impossible d'écrire %s\n", path); return -1; }

    uint32_t data_bytes = (uint32_t)(n * 2);
    uint32_t chunk      = 36 + data_bytes;

    fwrite("RIFF", 1, 4, fp);   put_u32(fp, chunk);
    fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);   put_u32(fp, 16);
    put_u16(fp, 1);                     /* PCM                     */
    put_u16(fp, 1);                     /* mono : un seul axe      */
    put_u32(fp, SAMPLE_RATE);
    put_u32(fp, SAMPLE_RATE * 2);       /* octets/s                */
    put_u16(fp, 2);                     /* alignement bloc         */
    put_u16(fp, 16);                    /* bits                    */
    fwrite("data", 1, 4, fp);   put_u32(fp, data_bytes);

    for (int64_t i = 0; i < n; i++) {
        double x = samples[i];
        if (x >  1.0) x =  1.0;         /* le mur dur du conteneur */
        if (x < -1.0) x = -1.0;
        int32_t v = (int32_t)lround(x * 32767.0);
        put_u16(fp, (uint16_t)(v & 0xFFFF));
    }

    fclose(fp);
    return 0;
}
