# Test ROMs (not committed)

Everything in this directory except this README is gitignored — drop the
test ROM set here. The harness takes any `.md`/`.bin`/`.gen` path, but the
regression matrix assumes:

| File (suggested name)  | Exercises |
|---|---|
| `sonic1.md`            | SEGA voice (YM2612 DAC PCM), FM music |
| `sonic3.md`            | DAC drums |
| `sor2.md`              | FM + shadow/highlight |
| `gunstar.md`           | shadow/highlight + heavy DMA |
| `columns.md`           | H32 (256-wide) mode |
| `xeno_crisis.md`       | SGDK / XGM driver (issue #11 repro) |
| `tf4.md`               | Thunder Force IV — worst-case load |
| any EUR ROM            | PAL timing (313 lines, 50 Hz) |

Example run:

```bash
./hosttest/build.sh
./hosttest/gen_host hosttest/roms/sonic1.md 600 60 hosttest/out
python3 hosttest/ppm2png.py hosttest/out     # PPM -> PNG
# listen: hosttest/out/mixed.wav (44.1 kHz), ym.wav / psg.wav (chip rate)
```
