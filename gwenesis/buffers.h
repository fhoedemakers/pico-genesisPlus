#ifndef _BUFFERS_H_
#define _BUFFERS_H_

extern unsigned char *M68K_RAM;
extern unsigned char *ZRAM;
extern unsigned char __aligned(4) *VRAM;
extern uint8_t (*SCREEN)[320];
extern int16_t *gwenesis_sn76489_buffer; 
void init_emulator_mem();
void free_emulator_mem();
#endif