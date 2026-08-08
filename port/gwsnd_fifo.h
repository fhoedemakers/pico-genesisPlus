/*
port/gwsnd_fifo.h — SPSC event FIFO between core0 (producer: bus/Z80 sound
register writes) and core1 (consumer: chip synthesis engine).

Power-of-two ring, indices only ever written by their own side, with a
data-memory barrier before publishing so the consumer never observes an
index ahead of its payload (same pattern as the pico_hdmi DI queue).
*/
#ifndef GWENESIS_PORT_GWSND_FIFO_H
#define GWENESIS_PORT_GWSND_FIFO_H

#include <stdint.h>

#if defined(GWENESIS_HOST) && GWENESIS_HOST != 0
#define gwsnd_dmb() __sync_synchronize()
#else
#include "hardware/sync.h"
#define gwsnd_dmb() __dmb()
#endif

enum gwsnd_event_kind {
    GWSND_EV_YM_W0 = 0, /* YM2612 ports 0..3: kind == port */
    GWSND_EV_YM_W1 = 1,
    GWSND_EV_YM_W2 = 2,
    GWSND_EV_YM_W3 = 3,
    GWSND_EV_PSG_W = 4,
    GWSND_EV_FRAME_END = 5, /* ts = system_clock at end of frame */
};

typedef struct {
    int32_t ts;
    uint8_t kind;
    uint8_t val;
    uint16_t pad;
} gwsnd_event_t;

#define GWSND_FIFO_LEN 2048 /* power of two; 16 KB */
#define GWSND_FIFO_MASK (GWSND_FIFO_LEN - 1)

typedef struct {
    gwsnd_event_t *ev;             /* GWSND_FIFO_LEN entries, heap (SRAM) */
    volatile uint32_t head;        /* producer writes */
    volatile uint32_t tail;        /* consumer writes */
    volatile uint32_t drop_count;  /* producer-side overflow drops */
} gwsnd_fifo_t;

static inline int gwsnd_fifo_level(const gwsnd_fifo_t *f)
{
    return (int)((f->head - f->tail) & 0xffffffffu);
}

/* Producer (core0). Returns 0 when the FIFO was full and the event dropped. */
static inline int gwsnd_fifo_push(gwsnd_fifo_t *f, int32_t ts, uint8_t kind, uint8_t val)
{
    uint32_t head = f->head;
    if (head - f->tail >= GWSND_FIFO_LEN) {
        return 0;
    }
    gwsnd_event_t *e = &f->ev[head & GWSND_FIFO_MASK];
    e->ts = ts;
    e->kind = kind;
    e->val = val;
    gwsnd_dmb();
    f->head = head + 1;
    return 1;
}

/* Consumer (core1). Returns 0 when empty. */
static inline int gwsnd_fifo_pop(gwsnd_fifo_t *f, gwsnd_event_t *out)
{
    uint32_t tail = f->tail;
    if (tail == f->head)
        return 0;
    *out = f->ev[tail & GWSND_FIFO_MASK];
    gwsnd_dmb();
    f->tail = tail + 1;
    return 1;
}

#endif
