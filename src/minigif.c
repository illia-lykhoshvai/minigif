#include "minigif.h"

#include "minigif_structs.h"

#define MAX(a,b) (a > b ? a : b)

// debug start
#include "esp_log.h"
#define TAG __FILENAME__
#define LOG(str, ...) ESP_LOGD(TAG, str, ##__VA_ARGS__)
#define LOG_ERR(str, ...) ESP_LOGE(TAG, str, ##__VA_ARGS__)
// debug end

// ------------------------
// POSSIBLE CALLBACKS START
// ------------------------
static void _open_file(gif_handle_t gif, const char *name)
{
    gif->f = fopen(name, "rb");
    
    if (gif->f == NULL) {
        LOG_ERR("fopen %s", name);
        return;
    }

    fseek(gif->f, 0, SEEK_SET);
}

static void _close_file(gif_handle_t gif)
{
    fclose(gif->f);
}

static size_t _read_bytes(gif_handle_t gif, uint8_t *val, uint16_t size)
{
    size_t read_amount = fread(val, sizeof(val[0]), size, gif->f);
    assert(size == read_amount && "read file failed");
    return read_amount;
}

static size_t _skip_bytes(gif_handle_t gif, uint16_t size)
{
    return fseek(gif->f, size, SEEK_CUR);
}
// ----------------------
// POSSIBLE CALLBACKS END
// ----------------------

static bool _check_gif_start(gif_handle_t gif)
{
    // check header and parse some fields
    char header[6];
    const char correct_version[] = "GIF89a";
    _read_bytes(gif, (uint8_t *)&header, sizeof(header));
    
    if (0 != memcmp(correct_version, header, sizeof(header))) {
        LOG_ERR("Wrong GIF header: %s, seek for %s", header, correct_version);
        return false;
    }
    
    gif_logical_screen_descr_t screen_descr = {0};
    _read_bytes(gif, (uint8_t *)&screen_descr, sizeof(screen_descr));

    bool global_color_table = (screen_descr.fields & LSD__FIELD_GCT_MASK) != 0;
    uint8_t color_resolution = ((screen_descr.fields & LSD__FIELD_CR_MASK) >> 4) + 1;
    bool sort = (screen_descr.fields & LSD__FIELD_SORT_MASK) != 0;
    uint16_t global_color_table_size = 1 << ((screen_descr.fields & LSD__FIELD_GCT_SZ_MASK) + 1);

    LOG("GIF fields: GCT:%d; CR=%d; SRT=%d; GCT_SZ=%d", global_color_table, color_resolution, sort, global_color_table_size);

    gif->cr = color_resolution;
    if (global_color_table) {
        gif->gct_active = true;
        gif->gct_size = global_color_table_size;   
        // global color table, right after the header
        uint16_t gct_buf_size = sizeof(gif_rgb_t) * global_color_table_size;
        _read_bytes(gif, (uint8_t *)&gif->gct[0], gct_buf_size);
    }

    return true;
}

gif_handle_t minigif_init(const char *filename, paint_cb_t paint_cb, void *user_data)
{
    assert(filename != NULL && paint_cb != NULL && "minigif_init params error");

    gif_handle_t gif = malloc(sizeof(gif_t));

    if (gif == NULL) {
        LOG_ERR("malloc(sizeof(gif_t)) failed");
        return NULL;
    }

    memset(gif, 0, sizeof(gif_t));

    gif->cb_user_data = user_data;
    gif->paint_cb = paint_cb;

    _open_file(gif, filename);

    if (!_check_gif_start(gif)) {
        free(gif);
        gif = NULL;
    }

    return gif;
}

bool _process_img(gif_handle_t gif) 
{
    bool gct_before_image = gif->gct_active;
    gif_img_descr_t img_descr = {0};
    _read_bytes(gif, (uint8_t *)&img_descr, sizeof(img_descr));
    LOG("image_header: [%d;%d] w=%d,h=%d", img_descr.x0, img_descr.y0, img_descr.w, img_descr.h);

    bool color_table = (img_descr.fields & IMG__FIELD_LCT_MASK) != 0;
    uint8_t interlaced = (img_descr.fields & IMG__FIELD_INTRLC_MASK) != 0;
    bool sort = (img_descr.fields & IMG__FIELD_SORT_MASK) != 0;
    uint16_t color_table_size = 1 << ((img_descr.fields & IMG__FIELD_LCT_SZ_MASK) + 1);

    LOG("image_header: LCT:%d; INTRL=%d; SRT=%d; LCT_SZ=%d", color_table, interlaced, sort, color_table_size);

    if (color_table) {
        gif->lct_size = color_table_size;
        // local color table, right after the image descriptior
        uint16_t lct_buf_size = sizeof(gif_rgb_t) * color_table_size;
        _read_bytes(gif, (uint8_t *)&gif->lct[0], lct_buf_size);
    }

    uint8_t lzw_min_code_sz;
    _read_bytes(gif, &lzw_min_code_sz, sizeof(lzw_min_code_sz));

    // discard_sub_blocks
    uint32_t total_image_size = 0;
    uint8_t block_sz = 0;
    do {
        _read_bytes(gif, &block_sz, sizeof(block_sz));
        total_image_size += block_sz;
        _skip_bytes(gif, block_sz);
    } while (block_sz);
    LOG("image length = %ld", total_image_size);

    gif->gct_active = gct_before_image;
    return true;
}

bool _process_ext(gif_handle_t gif)
{
    uint8_t ext_type = 0;
    uint8_t block_sz = 0;
    uint8_t block_buf[255];

    _read_bytes(gif, &ext_type, sizeof(ext_type));
    _read_bytes(gif, &block_sz, sizeof(block_sz));
    _read_bytes(gif, &block_buf[0], block_sz);

    LOG("Extension type 0x%.02x; block_sz=%d", ext_type, block_sz);
    switch(ext_type) {
        case GRAPHIC_CONTROL:
            assert(block_sz == sizeof(gif_gce_block_t));
            gif_gce_block_t *gce = (gif_gce_block_t *)&block_buf[0];
            LOG("GCE: fields=[disp:%d;usr:%d;transp:%d]; delay=%d; transp_col_index=%d",
                gce->fields & GCE__FIELD_DISP_METHOD_MASK, gce->fields & GCE__FIELD_USR_IN_MASK, gce->fields & GCE__FIELD_TRANSP_CLR_MASK,
                gce->delay_time, gce->transparent_col_index);
            
            gif->next_delay = gce->delay_time;
            if (gif->first_gce == 0) {
                gif->first_gce = ftell(gif->f) - block_sz - sizeof(block_sz) - sizeof(ext_type) - 1;
            }

            // skip block end
            _skip_bytes(gif, 1);
            break;
        case COMMENT:
            // print comment
            LOG("comment: %s", block_buf);
            // skip block end
            _skip_bytes(gif, 1);
            break;
        case APPLICATION:
            assert(block_sz == sizeof(gif_application_block_t));
            gif_application_block_t *app_block = (gif_application_block_t *)&block_buf[0];
            ESP_LOG_BUFFER_HEXDUMP(TAG, app_block->id, sizeof(app_block->id), ESP_LOG_INFO);
            ESP_LOG_BUFFER_HEXDUMP(TAG, app_block->auth_code, sizeof(app_block->auth_code), ESP_LOG_INFO);
            // discard_sub_blocks
            do {
                _read_bytes(gif, &block_sz, sizeof(block_sz));
                _skip_bytes(gif, block_sz);
            } while (block_sz);
            break;
        case PLAIN_TEXT:
            assert(0 && "Plain Text extension is not supported :(");
            break;
        default:
            LOG_ERR("Unrecognized extension type...");
            break;
    }
    return true;
}

gif_status_t minigif_render_frame(gif_handle_t gif)
{
    uint8_t block_type = 0;

    do {
        _read_bytes(gif, &block_type, sizeof(block_type));
        switch(block_type) {
            case TRAILER:
                LOG("TRAILER");
                return GIF_STATUS_GIF_END;
            case IMAGE_DESCRIPTOR:
                if (!_process_img(gif)) {
                    return GIF_STATUS_ERROR;
                }
                break;
            case EXTENSION_INTRO:
                if (!_process_ext(gif)) {
                    return GIF_STATUS_ERROR;
                }
                break;
            default:
                LOG_ERR("Unrecognized block type %.02x", block_type);
                return GIF_STATUS_ERROR;
        }
        if (gif->next_delay != 0 && block_type == IMAGE_DESCRIPTOR) {
            return GIF_STATUS_FRAME_END;
        }
    } while (block_type != TRAILER);

    return GIF_STATUS_GIF_END;
}

uint32_t minigif_get_frame_delay(gif_handle_t gif)
{
    gif->frame_delay = gif->next_delay;
    gif->next_delay = 0;
    return gif->frame_delay;
}

void minigif_rewind(gif_handle_t gif)
{
    fseek(gif->f, gif->first_gce, SEEK_SET);
}

void minigif_deinit(gif_handle_t gif)
{
    _close_file(gif);
    free(gif);
}
