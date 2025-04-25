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
#define LSD__FIELD_GCT_MASK     0x80 // Graphic Color Table flag
#define LSD__FIELD_CR_MASK      0x70 // Color resolution in bits = CR + 1
#define LSD__FIELD_SORT_MASK    0x08 // Colors sorted
#define LSD__FIELD_GCT_SZ_MASK  0x07 // GCT size = 2 ^ (GCT_SZ + 1)
    uint8_t fields;
    uint8_t bg_color_index;
    uint8_t pixel_aspect_ratio;
} __packed gif_logical_screen_descr_t;

typedef struct {
    uint16_t x0;
    uint16_t y0;
    uint16_t w;
    uint16_t h;
#define IMG__FIELD_LCT_MASK     0x80 // Local Color Table flag
#define IMG__FIELD_INTRLC_MASK  0x40 // Interlace
#define IMG__FIELD_SORT_MASK    0x20 // Colors sorted
#define IMG__FIELD_LCT_SZ_MASK  0x07 // LCT size = 2 ^ (GCT_SZ + 1)
    uint8_t fields;
} __packed gif_img_descr_t;

typedef struct {
#define GCE__FIELD_DISP_METHOD_MASK 0x28 // Disposal Method
#define GCE__FIELD_USR_IN_MASK      0x02 // User Input Flag
#define GCE__FIELD_TRANSP_CLR_MASK  0x01 // Transparent Color Flag
    uint8_t fields;
    uint16_t delay_time; // in 1/100 of second (10ms)
    uint8_t transparent_col_index;
} __packed gif_gce_block_t;

typedef struct {
    uint8_t id[8];
    uint8_t auth_code[3];
} __packed gif_application_block_t;

#endif // ifndef MINIGIF_STRUCTS_H
