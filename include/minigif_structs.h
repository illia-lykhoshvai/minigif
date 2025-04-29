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
    uint8_t r;
    uint8_t g;
    uint8_t b;
} gif_rgb_t;

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
} __attribute__((__packed__)) gif_logical_screen_descr_t;

typedef struct {
    union {
        uint8_t u8;
        struct { // !!! LSB-first order
            uint8_t transparent_flag : 1;
            uint8_t user_input_flag : 1;
            uint8_t disposal : 3;
            uint8_t _ : 3;
        } bits;
    } fields;
    uint16_t delay_time; // in 1/100 of second (10ms)
    uint8_t transparent_col_index;
} __attribute__((__packed__)) gif_gce_t;

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
} __attribute__((__packed__)) gif_img_descr_t;

typedef struct {
    uint8_t id[8];
    uint8_t auth_code[3];
} __attribute__((__packed__)) gif_application_block_t;

typedef struct {
    uint16_t grid_left_pos;
    uint16_t grid_top_pos;
    uint16_t grid_width;
    uint16_t grid_height;
    uint8_t char_width;
    uint8_t char_height;
    uint8_t foreground_color_index;
    uint8_t background_color_index;
} __attribute__((__packed__)) gif_plain_text_block_t;

typedef struct { 
    uint16_t prefix; 
    uint8_t suffix; 
} __attribute__((__packed__)) dict_cell_t;

#define MINIGIF_LZW_DICT_SIZE 4096

typedef struct {
    gif_img_descr_t *img_descr;

    uint16_t clear_code, end_code;
    uint8_t lzw_min_code_sz;
    dict_cell_t dict[MINIGIF_LZW_DICT_SIZE];

    uint8_t block_buffer[255];        // buffer for up to 255 bytes
    size_t block_size;                // current valid byte count
    size_t block_pos;                 // byte index in block
    uint8_t bit_buffer[2];            // to hold leftover bits across blocks
    int bit_count;
    int bit_val;
} lzw_context_t; // internal structure

#endif // ifndef MINIGIF_STRUCTS_H
