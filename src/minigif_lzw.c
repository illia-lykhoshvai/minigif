#include "minigif.h"

#define MINIGIF_LZW_STACK_SIZE 4096

// Read a single LZW code (bitstream spans blocks)
static int read_lzw_code(gif_handle_t gif, int code_size)
{
    while (gif->lzw_ctx.bit_count < code_size) {
        // If we consumed the block, read a new one
        if (gif->lzw_ctx.block_pos >= gif->lzw_ctx.block_size) {
            if (gif->cb.read(gif->f, (uint8_t *)&gif->lzw_ctx.block_size, sizeof(uint8_t)) != sizeof(uint8_t) || gif->lzw_ctx.block_size == 0) {
                return -1; // No more data
            }
            if (gif->cb.read(gif->f, gif->lzw_ctx.block_buffer, gif->lzw_ctx.block_size) != gif->lzw_ctx.block_size) {
                return -1; // No more data
            }
            gif->lzw_ctx.block_pos = 0;
        }

        uint8_t byte = gif->lzw_ctx.block_buffer[gif->lzw_ctx.block_pos++];
        gif->lzw_ctx.bit_val |= (byte << gif->lzw_ctx.bit_count);
        gif->lzw_ctx.bit_count += 8;
    }
    
    int code = gif->lzw_ctx.bit_val & ((1 << code_size) - 1);
    gif->lzw_ctx.bit_val >>= code_size;
    gif->lzw_ctx.bit_count -= code_size;
    return code;
}

int lzw_decompress(gif_handle_t gif)
{
    dict_cell_t *dict = &gif->lzw_ctx.dict[0];
    uint8_t stack[MINIGIF_LZW_STACK_SIZE];
    uint8_t *sp;

    int code_size = gif->lzw_ctx.lzw_min_code_sz + 1;
    int clear = (1 << gif->lzw_ctx.lzw_min_code_sz);
    int end = clear + 1;
    int next_code = end + 1;
    int max_code = (1 << code_size);

    // Initialize dictionary
    for (int i = 0; i < clear; i++) {
        dict[i].prefix = 0xFFF;
        dict[i].suffix = i;
    }

    sp = stack;
    int old_code = -1;
    int code;
    uint16_t x = gif->lzw_ctx.img_descr->x0, y = gif->lzw_ctx.img_descr->y0;

    gif->lzw_ctx.bit_val = 0;
    gif->lzw_ctx.bit_count = 0;
    gif->lzw_ctx.block_size = 0;
    gif->lzw_ctx.block_pos = 0;

    while ((code = read_lzw_code(gif, code_size)) >= 0) {
        if (code == clear) {
            code_size = gif->lzw_ctx.lzw_min_code_sz + 1;
            max_code = (1 << code_size);
            next_code = end + 1;
            old_code = -1;
            continue;
        }
        if (code == end)
            break;

        int in_code = code;
        if (code >= next_code) {
            *sp++ = dict[old_code].suffix;
            code = old_code;
        }

        while (code >= clear) {
            *sp++ = dict[code].suffix;
            code = dict[code].prefix;
        }
        *sp++ = dict[code].suffix;

        // Output pixels in reverse order from stack
        while (sp != stack) {
            uint8_t color_idx = *(--sp);
            if (gif->active_gce.fields.bits.transparent_flag && (color_idx == gif->active_gce.transparent_col_index)) {
                // do not draw anything as transparency is needed
            } else {
                gif_rgb_t color = gif->lct_size ? gif->lct[color_idx] : gif->gct[color_idx];
                gif->cb.painter(x, y, color, gif->cb.user_data);
            }
            if (++x >= gif->lzw_ctx.img_descr->w) {
                x = 0;
                if (++y >= gif->lzw_ctx.img_descr->h) {
                    return 0;
                }
            }
        }

        if (old_code != -1 && next_code < MINIGIF_LZW_DICT_SIZE) {
            dict[next_code].prefix = old_code;
            dict[next_code].suffix = dict[code].suffix;
            next_code++;
            if (next_code >= max_code && code_size < 12) {
                code_size++;
                max_code = (1 << code_size);
            }
        }
        old_code = in_code;
    }
    return 0;
}
