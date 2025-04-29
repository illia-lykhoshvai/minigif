#include "minigif.h"

#define MAX(a,b) (a > b ? a : b)

// debug start
#ifndef LOG_DEBUG
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(fmt, ...) ((void)0)
#endif
// debug end

extern int lzw_decompress(gif_handle_t gif);

static bool _check_gif_start(gif_handle_t gif)
{
    // check header and parse some fields
    char header[6];
    const char correct_version[] = "GIF89a";
    gif->cb.read(gif->f, (uint8_t *)&header, sizeof(header));
    
    if (0 != memcmp(correct_version, header, sizeof(header))) {
        LOG_ERROR("Wrong GIF header: %s, seek for %s", header, correct_version);
        return false;
    }
    
    gif_logical_screen_descr_t screen_descr = {0};
    gif->cb.read(gif->f, (uint8_t *)&screen_descr, sizeof(screen_descr));

    LOG_DEBUG("GIF fields: GCT:%d; CR=%d; SRT=%d; GCT_SZ=%d", 
        screen_descr.fields.bits.gct_flag, screen_descr.fields.bits.color_resolution, 
        screen_descr.fields.bits.sort, screen_descr.fields.bits.gct_size);

    if (screen_descr.fields.bits.gct_flag) {
        uint16_t gct_size = 1 << (screen_descr.fields.bits.gct_size + 1);
        gif->gct_size = gct_size;
        // global color table, right after the header
        uint16_t gct_buf_size = sizeof(gif_rgb_t) * gct_size;
        gif->cb.read(gif->f, (uint8_t *)&gif->gct[0], gct_buf_size);
    }

    return true;
}

gif_handle_t minigif_init(uint8_t buffer[sizeof(gif_t)], void *f, gif_cb_t callbacks)
{
    if (buffer == NULL || f == NULL) {
        return NULL;
    }

    gif_handle_t gif = (gif_handle_t)buffer;
    memset(gif, 0, sizeof(gif_t));

    gif->cb = callbacks;
    gif->f = f;

    if (!_check_gif_start(gif)) {
        gif = NULL;
    }

    return gif;
}

static bool _process_img(gif_handle_t gif) 
{
    gif_img_descr_t img_descr = {0};
    gif->cb.read(gif->f, (uint8_t *)&img_descr, sizeof(img_descr));
    LOG_DEBUG("image_header: [%d;%d] w=%d,h=%d", img_descr.x0, img_descr.y0, img_descr.w, img_descr.h);
    LOG_DEBUG("image_header: LCT:%d; INTRL=%d; SRT=%d; LCT_SZ=%d", 
        img_descr.fields.bits.lct_flag, img_descr.fields.bits.interlaced, 
        img_descr.fields.bits.sort, img_descr.fields.bits.lct_size);

    if (img_descr.fields.bits.interlaced) {
        LOG_DEBUG("image_header: interlacing not supported in this version");
        return false;
    }

    if (img_descr.fields.bits.lct_flag) {
        uint16_t color_table_size = 1 << (img_descr.fields.bits.lct_size + 1);
        gif->lct_size = color_table_size;
        // local color table, right after the image descriptior
        uint16_t lct_buf_size = sizeof(gif_rgb_t) * color_table_size;
        gif->cb.read(gif->f, (uint8_t *)&gif->lct[0], lct_buf_size);
    } else {
        gif->lct_size = 0;
    }

    uint8_t lzw_min_code_sz;
    gif->cb.read(gif->f, &lzw_min_code_sz, sizeof(lzw_min_code_sz));
    
    memset(&gif->lzw_ctx, 0, sizeof(gif->lzw_ctx));
    gif->lzw_ctx.img_descr = &img_descr;
    gif->lzw_ctx.lzw_min_code_sz = lzw_min_code_sz;
    gif->lzw_ctx.clear_code = (1 << lzw_min_code_sz);
    gif->lzw_ctx.end_code = (1 << lzw_min_code_sz) + 1;
    
    int res = lzw_decompress(gif);

    // skip until block end
    uint8_t last_byte;
    do {
        gif->cb.read(gif->f, &last_byte, sizeof(last_byte));    
    } while (last_byte != BLOCK_TERMINATOR);

    return res == 0;
}

static void _process_gce(gif_handle_t gif, gif_gce_t *gce)
{
    memcpy(&gif->active_gce, gce, sizeof(gif_gce_t));

    LOG_DEBUG("GCE: fields=[disp:%d;usr:%d;transp:%d]; delay=%d; transp_col_index=%d",
        gif->active_gce.fields.bits.disposal, gif->active_gce.fields.bits.user_input_flag, gif->active_gce.fields.bits.transparent_flag,
        gif->active_gce.delay_time, gif->active_gce.transparent_col_index);

    if (gif->first_gce_pos == 0) {
        gif->first_gce_pos = gif->cb.lseek(gif->f, 0, SEEK_CUR) - sizeof(gif_gce_t) - 3; // 3: sizeof(ext_type) + sizeof(block_sz) + 1
    }
}

static void skip_subblocks(gif_handle_t gif)
{
    uint8_t sub_block_sz = 0;
    do {
        gif->cb.read(gif->f, &sub_block_sz, sizeof(sub_block_sz));
        gif->cb.lseek(gif->f, sub_block_sz, SEEK_CUR);
    } while (sub_block_sz);
}

static bool _process_ext(gif_handle_t gif)
{
    uint8_t ext_type = 0;
    uint8_t block_sz = 0;
    uint8_t block_buf[255];

    gif->cb.read(gif->f, &ext_type, sizeof(ext_type));
    gif->cb.read(gif->f, &block_sz, sizeof(block_sz));
    gif->cb.read(gif->f, &block_buf[0], block_sz);

    LOG_DEBUG("Extension type 0x%.02x; block_sz=%d", ext_type, block_sz);
    switch(ext_type) {
        case GRAPHIC_CONTROL:
            if (block_sz != sizeof(gif_gce_t)) {
                return false;
            }

            _process_gce(gif, (gif_gce_t *)&block_buf[0]);
            gif->cb.lseek(gif->f, 1, SEEK_CUR); // skip block end
            break;
        case COMMENT:
            LOG_DEBUG("comment: %s", block_buf);
            gif->cb.lseek(gif->f, 1, SEEK_CUR); // skip block end
            break;
        case APPLICATION:
            if (block_sz != sizeof(gif_application_block_t)) {
                return false;
            }
            
            skip_subblocks(gif);
            break;
        case PLAIN_TEXT:
            if (block_sz != sizeof(gif_plain_text_block_t)) {
                return false;
            }

            skip_subblocks(gif);
            break;
        default:
            LOG_ERROR("Unrecognized extension type...");
            break;
    }
    return true;
}

gif_status_t minigif_render_frame(gif_handle_t gif)
{
    uint8_t block_type = 0;

    do {
        gif->cb.read(gif->f, &block_type, sizeof(block_type));
        switch(block_type) {
            case TRAILER:
                return GIF_STATUS_GIF_END;
            case IMAGE_DESCRIPTOR:
                return _process_img(gif) ? GIF_STATUS_FRAME_END : GIF_STATUS_ERROR;
            case EXTENSION_INTRO:
                if (!_process_ext(gif)) {
                    return GIF_STATUS_ERROR;
                }
                break;
            default:
                LOG_ERROR("Unrecognized block type %.02x @ 0x%.08lx", block_type, gif->cb.lseek(gif->f, 0, SEEK_CUR) - sizeof(block_type));
                return GIF_STATUS_ERROR;
        }
    } while (block_type != TRAILER);

    return GIF_STATUS_ERROR;
}

uint32_t minigif_get_frame_delay(gif_handle_t gif)
{
    return gif->active_gce.delay_time;
}

void minigif_rewind(gif_handle_t gif)
{
    gif->cb.lseek(gif->f, gif->first_gce_pos, SEEK_SET);
}
