#include <stdio.h>
#include <stdint.h>

uint8_t paletteBrightness[] = { 0, 52, 87, 116, 144, 172, 206, 255 };

void genPalette() {
    for (int i = 0; i < 8; i++) {
        printf("0x%02X, ", paletteBrightness[i]);
    }
    printf("\n");
}
int main() {
    // printf const WORD __not_in_flash_func(NesPalette)[64] = {
    printf("const WORD __not_in_flash_func(GenesisPalette)[512] = {\n");
    for (int i = 0; i < 8; i++) {
        int r = paletteBrightness[i];
        for (  int j = 0; j < 8; j++) {
            int g = paletteBrightness[j];
            for (int k = 0; k < 8; k++) {
                int b = paletteBrightness[k];
                uint16_t RGB444color = ( (r >> 4 ) << 8 ) | ( (g >> 4) << 4 ) | (b >> 4);
                printf("0x%03X, ", RGB444color);
            }
            printf("\n");
        }
    }
    printf("};\n");

    return 0;
}