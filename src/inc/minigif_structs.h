#ifndef MINIGIF_STRUCTS_H
#define MINIGIF_STRUCTS_H

#include <stdint.h>

typedef enum {
    BLOCK_TERMINATOR = 0x00,
    EXTENSION_INTRO = 0x21,
    IMAGE_DESCRIPTOR = 0x2C,
    TRAILER = 0x3B,
    // extensions:
    PLAIN_TEXT = 0x01,
    GRAPHIC_CONTROL = 0xF9,
    COMMENT = 0xFE,
    APPLICATION = 0xFF,
} block_id_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    union {
        uint8_t u8;
        struct { // !!! LSB-first order
            uint8_t gct_size : 3;
            uint8_t sort : 1;
            uint8_t color_resolution : 3;
            uint8_t gct_flag : 1;
        } bits;
    } fields;
    uint8_t bg_color_index;
    uint8_t pixel_aspect_ratio;
} __packed gif_logical_screen_descr_t;

typedef struct {
    uint16_t x0;
    uint16_t y0;
    uint16_t w;
    uint16_t h;
    union {
        uint8_t u8;
        struct { // !!! LSB-first order
            uint8_t lct_size : 3;
            uint8_t _ : 2;
            uint8_t sort : 1;
            uint8_t interlaced : 1;
            uint8_t lct_flag : 1;
        } bits;
    } fields;
} __packed gif_img_descr_t;

typedef struct {
    uint8_t id[8];
    uint8_t auth_code[3];
} __packed gif_application_block_t;

#endif // ifndef MINIGIF_STRUCTS_H
