/*
hosttest/host_main.c — headless Linux harness for the gwenesis core.

Runs the exact same core sources and frame loop (port/frame_loop.inc) as
the pico firmware, with the display in RGB565, and dumps:
  - PPM frames every N frames        -> <outdir>/frame_NNNNN.ppm
  - 44.1 kHz stereo mixed audio      -> <outdir>/mixed.wav (real resampler)
  - raw chip-rate mono per-chip WAVs -> <outdir>/ym.wav, <outdir>/psg.wav

Usage:
    gen_host <rom.md|.bin|.gen> <total-frames> <dump-every-N> [outdir]

Sequential-launch mode (reproduces "second game is broken" bugs, where
state survives in the core between games because the firmware starts a new
game without rebooting):

    GEN_FIRST_ROM=<rom> gen_host <rom2> <frames> <dump-every> [outdir]

runs <rom> to completion first, tears everything down exactly as the
firmware does (gwsnd_shutdown + free_emulator_mem), then launches <rom2>.
Its output must be byte-identical to launching <rom2> on its own.

Input injection (frame ranges, inclusive):
    GEN_PRESS_START="120:180"   hold START on pad 0
    GEN_PRESS_A="200:220"       hold A on pad 0
    GEN_PRESS_B / GEN_PRESS_C   likewise

Convert PPMs: python3 hosttest/ppm2png.py <outdir>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "gwenesis_bus.h"
#include "gwenesis_vdp.h"
#include "gwenesis_io.h"
#include "m68k.h"
#include "z80inst.h"
#include "ym2612.h"
#include "gwenesis_sn76489.h"

#include "gwsnd.h"
#include "buffers.h"
#include "wav.h"

#include "frame_loop.inc"

#define FB_W 320
#define FB_H 240

static uint16_t framebuffer[FB_W * FB_H];

static wav_writer wav_mixed, wav_ym, wav_psg;

/* ------------------------- input injection ------------------------- */

extern unsigned char button_state[3];

typedef struct {
    int from, to, bit;
} press_range;

static press_range presses[8];
static int press_count;
static int current_frame_no;

static void parse_press(const char *env, int bit)
{
    const char *v = getenv(env);
    if (!v || press_count >= 8)
        return;
    int from = 0, to = 0;
    if (sscanf(v, "%d:%d", &from, &to) == 2) {
        presses[press_count].from = from;
        presses[press_count].to = to;
        presses[press_count].bit = bit;
        press_count++;
    }
}

/* Core callback: refresh button_state (active low, S A C B R L D U). */
void gwenesis_io_get_buttons(void)
{
    unsigned char pressed = 0;
    for (int i = 0; i < press_count; i++) {
        if (current_frame_no >= presses[i].from && current_frame_no <= presses[i].to)
            pressed |= (unsigned char)(1u << presses[i].bit);
    }
    button_state[0] = (unsigned char)~pressed;
    button_state[1] = 0xff;
    button_state[2] = 0xff;
}

/* --------------------------- audio sinks --------------------------- */

static void audio_out(int16_t l, int16_t r)
{
    int16_t s[2] = {l, r};
    wav_write(&wav_mixed, s, 2);
}

static void frame_tap(const int16_t *ym, const int16_t *psg, int samples)
{
    wav_write(&wav_ym, ym, samples);
    wav_write(&wav_psg, psg, samples);
}

/* --------------------------- video dump ---------------------------- */

static void dump_ppm(const char *outdir, int frame)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/frame_%05d.ppm", outdir, frame);
    FILE *f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", FB_W, FB_H);
    for (int i = 0; i < FB_W * FB_H; i++) {
        uint16_t px = framebuffer[i];
        uint8_t rgb[3];
        rgb[0] = (uint8_t)(((px >> 11) & 0x1f) << 3);
        rgb[1] = (uint8_t)(((px >> 5) & 0x3f) << 2);
        rgb[2] = (uint8_t)((px & 0x1f) << 3);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* ---------------------------- ROM load ----------------------------- */

static const unsigned char *load_rom(const char *path, size_t *size_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0x200) {
        fclose(f);
        fprintf(stderr, "ROM too small\n");
        return NULL;
    }

    /* The core masks fetches with pow2ceil(size)-1, so back the image with
       a pow2-rounded buffer (+4 for the 32-bit fetch overhang). */
    size_t pot = 1;
    while (pot < (size_t)sz)
        pot <<= 1;
    unsigned char *rom = calloc(1, pot + 4);
    if (fread(rom, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        free(rom);
        fprintf(stderr, "short read\n");
        return NULL;
    }
    fclose(f);

    /* pico_shared byte-swaps Genesis ROMs at load; replicate. */
    for (long i = 0; i + 1 < sz; i += 2) {
        unsigned char t = rom[i];
        rom[i] = rom[i + 1];
        rom[i + 1] = t;
    }
    *size_out = (size_t)sz;
    return rom;
}

/* ------------------------------ main ------------------------------- */

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <rom> <total-frames> <dump-every-N> [outdir]\n",
                argv[0]);
        return 2;
    }
    const char *rom_path = argv[1];
    int total_frames = atoi(argv[2]);
    int dump_every = atoi(argv[3]);
    const char *outdir = argc > 4 ? argv[4] : "out";
    mkdir(outdir, 0755);

    parse_press("GEN_PRESS_START", 7);
    parse_press("GEN_PRESS_A", 6);
    parse_press("GEN_PRESS_C", 5);
    parse_press("GEN_PRESS_B", 4);

    /* Optional warm-up launch: run a different game first and tear it
       down, so this run starts from whatever state the core left behind. */
    const char *first_rom = getenv("GEN_FIRST_ROM");
    if (first_rom && *first_rom) {
        size_t first_size = 0;
        const unsigned char *first = load_rom(first_rom, &first_size);
        if (!first)
            return 1;
        printf("=== warm-up launch: %s ===\n", first_rom);
        if (!init_emulator_mem()) {
            fprintf(stderr, "out of memory\n");
            return 1;
        }
        load_cartridge(first, first_size);
        power_on();
        reset_emulation();
        gwsnd_init(0, 0);
        for (int f = 0; f < 300; f++) {
            current_frame_no = f;
            gwsnd_set_pal(gwenesis_frame_get_config());
            int m = (FB_H - screen_height) / 2;
            gwenesis_vdp_set_buffer(&framebuffer[m * FB_W]);
            gwenesis_frame_run(1);
        }
        /* Exactly the firmware's teardown order. */
        gwsnd_shutdown();
        free_emulator_mem();
        memset(framebuffer, 0, sizeof(framebuffer));
        free((void *)first);
        printf("=== warm-up done, now launching %s ===\n", rom_path);
    }

    size_t rom_size = 0;
    const unsigned char *rom = load_rom(rom_path, &rom_size);
    if (!rom)
        return 1;

    if (!init_emulator_mem()) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/mixed.wav", outdir);
    wav_open(&wav_mixed, path, 44100, 2);
    snprintf(path, sizeof(path), "%s/ym.wav", outdir);
    wav_open(&wav_ym, path, GWENESIS_AUDIO_FREQ_NTSC, 1);
    snprintf(path, sizeof(path), "%s/psg.wav", outdir);
    wav_open(&wav_psg, path, GWENESIS_AUDIO_FREQ_NTSC, 1);

    load_cartridge(rom, rom_size);
    power_on();
    reset_emulation();

    gwsnd_set_output(audio_out);
    gwsnd_set_frame_tap(frame_tap);
    gwsnd_init(0, 0);

    for (int frame = 0; frame < total_frames; frame++) {
        current_frame_no = frame;

        int is_pal = gwenesis_frame_get_config();
        gwsnd_set_pal(is_pal);
        int margin = (FB_H - screen_height) / 2;
        gwenesis_vdp_set_buffer(&framebuffer[margin * FB_W]);

        gwenesis_frame_run(1);

        if (dump_every > 0 && frame % dump_every == 0)
            dump_ppm(outdir, frame);
    }

    wav_close(&wav_mixed);
    wav_close(&wav_ym);
    wav_close(&wav_psg);
    gwsnd_shutdown();
    free_emulator_mem();

    printf("ran %d frames, audio: %s/mixed.wav ym.wav psg.wav\n",
           total_frames, outdir);
    return 0;
}
