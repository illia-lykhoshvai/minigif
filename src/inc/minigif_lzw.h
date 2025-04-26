#ifndef MINIGIF_LZW_H
#define MINIGIF_LZW_H

#include "stdint.h"

#include "minigif.h"
#include "minigif_structs.h"

typedef struct __packed { 
    uint16_t prefix; 
    uint8_t suffix; 
} dict_cell_t;

typedef struct {
    gif_handle_t gif;
    gif_img_descr_t *img_descr;

    uint8_t lzw_min_code_sz;
    uint16_t clear_code, end_code;

    uint8_t block_buffer[255];        // buffer for up to 255 bytes
    size_t block_size;                // current valid byte count
    size_t block_pos;                 // byte index in block
    uint8_t bit_buffer[2];            // to hold leftover bits across blocks
    int bit_count;
    int bit_val;
} lzw_context_t;

int lzw_decompress(lzw_context_t *ctx);

#endif // ifndef MINIGIF_LZW_H
