/* hosttest/wav.h — minimal streaming 16-bit PCM WAV writer. */
#ifndef GEN_HOST_WAV_H
#define GEN_HOST_WAV_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    FILE *f;
    uint32_t data_bytes;
    uint32_t sample_rate;
    uint16_t channels;
} wav_writer;

static int wav_open(wav_writer *w, const char *path, uint32_t sample_rate,
                    uint16_t channels)
{
    memset(w, 0, sizeof(*w));
    w->f = fopen(path, "wb");
    if (!w->f)
        return -1;
    w->sample_rate = sample_rate;
    w->channels = channels;
    /* placeholder header, patched in wav_close */
    uint8_t hdr[44] = {0};
    fwrite(hdr, 1, sizeof(hdr), w->f);
    return 0;
}

static void wav_write(wav_writer *w, const int16_t *samples, int count)
{
    if (!w->f || count <= 0)
        return;
    fwrite(samples, sizeof(int16_t), (size_t)count, w->f);
    w->data_bytes += (uint32_t)count * 2;
}

static void wav_put32(uint8_t *p, uint32_t v)
{
    p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static void wav_put16(uint8_t *p, uint16_t v)
{
    p[0] = v; p[1] = v >> 8;
}

static void wav_close(wav_writer *w)
{
    if (!w->f)
        return;
    uint8_t hdr[44];
    memcpy(hdr, "RIFF", 4);
    wav_put32(hdr + 4, 36 + w->data_bytes);
    memcpy(hdr + 8, "WAVEfmt ", 8);
    wav_put32(hdr + 16, 16);
    wav_put16(hdr + 20, 1); /* PCM */
    wav_put16(hdr + 22, w->channels);
    wav_put32(hdr + 24, w->sample_rate);
    wav_put32(hdr + 28, w->sample_rate * w->channels * 2);
    wav_put16(hdr + 32, (uint16_t)(w->channels * 2));
    wav_put16(hdr + 34, 16);
    memcpy(hdr + 36, "data", 4);
    wav_put32(hdr + 40, w->data_bytes);
    fseek(w->f, 0, SEEK_SET);
    fwrite(hdr, 1, sizeof(hdr), w->f);
    fclose(w->f);
    w->f = NULL;
}

#endif
