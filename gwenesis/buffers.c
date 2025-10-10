#include "pico.h"
#include "bus/gwenesis_bus.h"
#include "vdp/gwenesis_vdp.h"
#include "buffers.h"
#include <stdlib.h>

unsigned char *M68K_RAM;
unsigned char *ZRAM; // [MAX_Z80_RAM_SIZE];
unsigned char *VRAM; // [VRAM_MAX_SIZE];
// Declare SCREEN as a pointer to an array of 320 uint8_t elements
// uint8_t (*SCREEN)[320];
int16_t *gwenesis_sn76489_buffer; // [GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2];  // 888 = NTSC, PAL = 1056 (too big) //GWENESIS_AUDIO_BUFFER_LENGTH_PAL];
extern void * frens_f_malloc(size_t size);
extern void frens_f_free(void *ptr);
extern void * frens_f_realloc(void *ptr, size_t newSize);
void init_emulator_mem() {
#if 0
    M68K_RAM = (unsigned char *)frens_f_malloc(MAX_RAM_SIZE);
    ZRAM = (unsigned char *)frens_f_malloc(MAX_Z80_RAM_SIZE);
    VRAM = (unsigned char *)frens_f_malloc(VRAM_MAX_SIZE);
    // SCREEN = (uint8_t (*)[320])malloc(240 * 320 * sizeof(uint8_t));
    //SCREEN = NULL;
    gwenesis_sn76489_buffer = (int16_t *)frens_f_malloc(GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2 * sizeof(int16_t));
#else
    M68K_RAM = (unsigned char *)malloc(MAX_RAM_SIZE);
    ZRAM = (unsigned char *)malloc(MAX_Z80_RAM_SIZE);
    VRAM = (unsigned char *)malloc(VRAM_MAX_SIZE);
    // SCREEN = (uint8_t (*)[320])malloc(240 * 320 * sizeof(uint8_t));
    //SCREEN = NULL;
    gwenesis_sn76489_buffer = (int16_t *)malloc(GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2 * sizeof(int16_t));
#endif
    // init all buffers
    memset(M68K_RAM, 0, MAX_RAM_SIZE);
    memset(ZRAM, 0, MAX_Z80_RAM_SIZE);
    memset(VRAM, 0, VRAM_MAX_SIZE);
    // FH memset(SCREEN, 0, 240 * 320 * sizeof(uint8_t));
    memset(gwenesis_sn76489_buffer, 0, GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2 * sizeof(int16_t));
}

void free_emulator_mem() {
#if 0
    frens_f_free(M68K_RAM);
    frens_f_free(ZRAM);
    frens_f_free(VRAM);
    //free(SCREEN);
    frens_f_free(gwenesis_sn76489_buffer);
#else
    free(M68K_RAM);
    free(ZRAM);
    free(VRAM);
    //free(SCREEN);
    free(gwenesis_sn76489_buffer);
#endif
}