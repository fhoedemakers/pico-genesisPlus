/*
port/savestate_stubs.c — no-op SaveState primitives.

The gwenesis core's *_save_state()/*_load_state() functions call these
host-provided primitives (on the Game & Watch they serialize to flash).
Savestates are not implemented in this port yet; these stubs keep the core
linkable. When savestates are added, replace this file with a real
implementation (CRC-keyed files on SD via pico_shared's save-state menu).
*/
#include "gwenesis_savestate.h"

struct SaveState {
    int dummy;
};

static SaveState dummy_state;

bool initLoadGwenesisState(unsigned char *srcBuffer)
{
    (void)srcBuffer;
    return false;
}

int saveGwenesisState(unsigned char *destBuffer, int save_size)
{
    (void)destBuffer;
    (void)save_size;
    return 0;
}

int loadGwenesisState(unsigned char *srcBuffer)
{
    (void)srcBuffer;
    return 0;
}

SaveState *saveGwenesisStateOpenForRead(const char *fileName)
{
    (void)fileName;
    return &dummy_state;
}

SaveState *saveGwenesisStateOpenForWrite(const char *fileName)
{
    (void)fileName;
    return &dummy_state;
}

int saveGwenesisStateGet(SaveState *state, const char *tagName)
{
    (void)state;
    (void)tagName;
    return 0;
}

void saveGwenesisStateSet(SaveState *state, const char *tagName, int value)
{
    (void)state;
    (void)tagName;
    (void)value;
}

void saveGwenesisStateGetBuffer(SaveState *state, const char *tagName, void *buffer, int length)
{
    (void)state;
    (void)tagName;
    (void)buffer;
    (void)length;
}

void saveGwenesisStateSetBuffer(SaveState *state, const char *tagName, void *buffer, int length)
{
    (void)state;
    (void)tagName;
    (void)buffer;
    (void)length;
}
