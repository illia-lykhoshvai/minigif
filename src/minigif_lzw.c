#include "minigif_lzw.h"

#define GIF_MAX_STACK_SIZE 4096
#define GIF_MAX_DICT_SIZE 4096

extern size_t _read_bytes(gif_handle_t gif, uint8_t *val, uint16_t size);
extern size_t _read_byte(gif_handle_t gif, uint8_t *val);

// Read a single LZW code (bitstream spans blocks)
static int read_lzw_code(lzw_context_t *ctx, int code_size)
{
    while (ctx->bit_count < code_size) {
        // If we consumed the block, read a new one
        if (ctx->block_pos >= ctx->block_size) {
            if (_read_byte(ctx->gif, (uint8_t *)&ctx->block_size) != 1 || ctx->block_size == 0) {
                return -1; // No more data
            }
            if (_read_bytes(ctx->gif, ctx->block_buffer, ctx->block_size) != ctx->block_size) {
                return -1; // No more data
            }
            ctx->block_pos = 0;
        }

        uint8_t byte = ctx->block_buffer[ctx->block_pos++];
        ctx->bit_val |= (byte << ctx->bit_count);
        ctx->bit_count += 8;
    }
    
    int code = ctx->bit_val & ((1 << code_size) - 1);
    ctx->bit_val >>= code_size;
    ctx->bit_count -= code_size;
    return code;
}

int lzw_decompress(lzw_context_t *ctx)
{
    dict_cell_t dict[GIF_MAX_DICT_SIZE];
    uint8_t stack[GIF_MAX_STACK_SIZE];
    uint8_t *sp;

    int code_size = ctx->lzw_min_code_sz + 1;
    int clear = (1 << ctx->lzw_min_code_sz);
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
    uint16_t x = ctx->img_descr->x0, y = ctx->img_descr->y0;

    ctx->bit_val = 0;
    ctx->bit_count = 0;
    ctx->block_size = 0;
    ctx->block_pos = 0;

    while ((code = read_lzw_code(ctx, code_size)) >= 0) {
        if (code == clear) {
            code_size = ctx->lzw_min_code_sz + 1;
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
            if (ctx->gif->active_gce.fields.bits.transparent_flag && (color_idx == ctx->gif->active_gce.transparent_col_index)) {
                // do not draw anything as transparency is needed
            } else {
                gif_rgb_t color = ctx->gif->lct_size ? ctx->gif->lct[color_idx] : ctx->gif->gct[color_idx];
                ctx->gif->painter(x, y, color, ctx->gif->user_data);
            }
            if (++x >= ctx->img_descr->w) {
                x = 0;
                if (++y >= ctx->img_descr->h)
                return 0;
            }
        }

        if (old_code != -1 && next_code < GIF_MAX_DICT_SIZE) {
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
