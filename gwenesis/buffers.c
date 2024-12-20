#include "pico.h"
#include "bus/gwenesis_bus.h"
#include "vdp/gwenesis_vdp.h"
#include "buffers.h"
#include <stdlib.h>
unsigned char *M68K_RAM;
unsigned char *ZRAM; // [MAX_Z80_RAM_SIZE];
unsigned char *VRAM; // [VRAM_MAX_SIZE];
void init_emulator_mem() {
    M68K_RAM = (unsigned char *)malloc(MAX_RAM_SIZE);
    ZRAM = (unsigned char *)malloc(MAX_Z80_RAM_SIZE);
    VRAM = (unsigned char *)malloc(VRAM_MAX_SIZE);
}

void free_emulator_mem() {
    free(M68K_RAM);
    free(ZRAM);
    free(VRAM);
}