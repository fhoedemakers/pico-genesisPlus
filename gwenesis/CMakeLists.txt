# Vendored gwenesis core (upstream github.com/bzhxx/gwenesis @ 168e466)
# plus the port patches documented in PORTING.md.
#
# Sources are listed explicitly: the Z80 debugger files (Debug.c/ConDebug.c)
# from upstream must never be compiled, and GLOBs hide mistakes.
add_library(gwenesis INTERFACE)

target_sources(gwenesis INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/bus/gwenesis_bus.c
    ${CMAKE_CURRENT_LIST_DIR}/cpus/M68K/m68kcpu.c
    ${CMAKE_CURRENT_LIST_DIR}/cpus/Z80/Z80.c
    ${CMAKE_CURRENT_LIST_DIR}/io/gwenesis_io.c
    ${CMAKE_CURRENT_LIST_DIR}/savestate/gwenesis_savestate.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/ym2612.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/gwenesis_sn76489.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/z80inst.c
    ${CMAKE_CURRENT_LIST_DIR}/vdp/gwenesis_vdp_mem.c
    ${CMAKE_CURRENT_LIST_DIR}/vdp/gwenesis_vdp_gfx.c
)

target_include_directories(gwenesis INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/bus
    ${CMAKE_CURRENT_LIST_DIR}/cpus/M68K
    ${CMAKE_CURRENT_LIST_DIR}/cpus/Z80
    ${CMAKE_CURRENT_LIST_DIR}/io
    ${CMAKE_CURRENT_LIST_DIR}/savestate
    ${CMAKE_CURRENT_LIST_DIR}/sound
    ${CMAKE_CURRENT_LIST_DIR}/vdp
)

target_compile_definitions(gwenesis INTERFACE
    GWENESIS_PICO=1
    # YM2612 LUTs get per-game RAM working copies (~46 KB heap): reading
    # them from XIP flash on every sample contends with the M68K dispatch
    # tables / PSRAM ROM on the shared QMI port and costs core0 frames in
    # DAC-heavy games. (The host harness keeps them const-in-.rodata.)
    GWENESIS_LUTS_IN_RAM=1
    # Upstream's default 61376-entry M68K dispatch/cycle tables are
    # truncated: opcodes >= 0xEFC0 (line-F traps) would jump out of bounds.
    # The _full variants are complete (320 KB of const data in flash).
    TABLES_FULL=1
    # The core tests these with bare #if !=0 — they must always be defined.
    GNW_TARGET_MARIO=0
    GNW_TARGET_ZELDA=0
    # Z80 speed: upstream MCLK/14 (~7% fast). 15 is hardware-exact; A/B
    # tested via -DGWENESIS_Z80_DIVISOR=15 (affects XGM1 PCM pitch).
    GWENESIS_Z80_DIVISOR=14
)

# Upstream relied on per-file "#pragma GCC optimize(Ofast)" under the GNW
# defines; give the hot translation units a fixed -O2 instead.
set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/cpus/M68K/m68kcpu.c
    ${CMAKE_CURRENT_LIST_DIR}/cpus/Z80/Z80.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/ym2612.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/gwenesis_sn76489.c
    ${CMAKE_CURRENT_LIST_DIR}/sound/z80inst.c
    ${CMAKE_CURRENT_LIST_DIR}/vdp/gwenesis_vdp_mem.c
    ${CMAKE_CURRENT_LIST_DIR}/vdp/gwenesis_vdp_gfx.c
    ${CMAKE_CURRENT_LIST_DIR}/bus/gwenesis_bus.c
    PROPERTIES COMPILE_OPTIONS "-O2"
)
