#include "pico.h"
#include "bus/gwenesis_bus.h"
#include "vdp/gwenesis_vdp.h"
#include "buffers.h"
#include <stdlib.h>
unsigned char *M68K_RAM;
unsigned char *ZRAM; // [MAX_Z80_RAM_SIZE];
unsigned char *VRAM; // [VRAM_MAX_SIZE];
// Declare SCREEN as a pointer to an array of 320 uint8_t elements
uint8_t (*SCREEN)[320];
int16_t *gwenesis_sn76489_buffer; // [GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2];  // 888 = NTSC, PAL = 1056 (too big) //GWENESIS_AUDIO_BUFFER_LENGTH_PAL];

void init_emulator_mem() {
    M68K_RAM = (unsigned char *)malloc(MAX_RAM_SIZE);
    ZRAM = (unsigned char *)malloc(MAX_Z80_RAM_SIZE);
    VRAM = (unsigned char *)malloc(VRAM_MAX_SIZE);
    SCREEN = (uint8_t (*)[320])malloc(240 * 320 * sizeof(uint8_t));
    gwenesis_sn76489_buffer = (int16_t *)malloc(GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2 * sizeof(int16_t));
    // init all buffers
    memset(M68K_RAM, 0, MAX_RAM_SIZE);
    memset(ZRAM, 0, MAX_Z80_RAM_SIZE);
    memset(VRAM, 0, VRAM_MAX_SIZE);
    memset(SCREEN, 0, 240 * 320 * sizeof(uint8_t));
    memset(gwenesis_sn76489_buffer, 0, GWENESIS_AUDIO_BUFFER_LENGTH_NTSC * 2 * sizeof(int16_t));
}

void free_emulator_mem() {
    free(M68K_RAM);
    free(ZRAM);
    free(VRAM);
    free(SCREEN);
    free(gwenesis_sn76489_buffer);
}