#include "minigif.h"
#include "minigif_structs.h"
#include "minigif_lzw.h"

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

size_t _read_bytes(gif_handle_t gif, uint8_t *val, uint16_t size)
{
    size_t read_amount = fread(val, sizeof(val[0]), size, gif->f);
    assert(size == read_amount && "read file failed");
    return read_amount;
}

size_t _read_byte(gif_handle_t gif, uint8_t *val)
{
    return _read_bytes(gif, val, sizeof(uint8_t));
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
        gif->gct_size = global_color_table_size;
        // global color table, right after the header
        uint16_t gct_buf_size = sizeof(gif_rgb_t) * global_color_table_size;
        _read_bytes(gif, (uint8_t *)&gif->gct[0], gct_buf_size);
    }

    return true;
}

gif_handle_t minigif_init(const char *filename, painter_cb_t painter, void *user_data)
{
    assert(filename != NULL && painter != NULL && "minigif_init params error");

    gif_handle_t gif = malloc(sizeof(gif_t));

    if (gif == NULL) {
        LOG_ERR("malloc(sizeof(gif_t)) failed");
        return NULL;
    }

    memset(gif, 0, sizeof(gif_t));

    gif->user_data = user_data;
    gif->painter = painter;

    _open_file(gif, filename);

    if (!_check_gif_start(gif)) {
        free(gif);
        gif = NULL;
    }

    return gif;
}

bool _process_img(gif_handle_t gif) 
{
    gif_img_descr_t img_descr = {0};
    _read_bytes(gif, (uint8_t *)&img_descr, sizeof(img_descr));
    LOG("image_header: [%d;%d] w=%d,h=%d", img_descr.x0, img_descr.y0, img_descr.w, img_descr.h);

    bool color_table = (img_descr.fields & IMG__FIELD_LCT_MASK) != 0;
    uint8_t interlaced = (img_descr.fields & IMG__FIELD_INTRLC_MASK) != 0;
    bool sort = (img_descr.fields & IMG__FIELD_SORT_MASK) != 0;
    uint16_t color_table_size = 1 << ((img_descr.fields & IMG__FIELD_LCT_SZ_MASK) + 1);

    LOG("image_header: LCT:%d; INTRL=%d; SRT=%d; LCT_SZ=%d", color_table, interlaced, sort, color_table_size);

    assert(interlaced == false);

    if (color_table) {
        gif->lct_size = color_table_size;
        // local color table, right after the image descriptior
        uint16_t lct_buf_size = sizeof(gif_rgb_t) * color_table_size;
        _read_bytes(gif, (uint8_t *)&gif->lct[0], lct_buf_size);
    } else {
        gif->lct_size = 0;
    }

    uint8_t lzw_min_code_sz;
    _read_byte(gif, &lzw_min_code_sz);
    
    lzw_context_t decoder = {
        .gif = gif,
        .img_descr = &img_descr,
        .lzw_min_code_sz = lzw_min_code_sz,
        .clear_code = (1 << lzw_min_code_sz),
        .end_code = (1 << lzw_min_code_sz) + 1,
        .block_size = 0,
        .block_pos = 0,
        .bit_val = 0,
        .bit_count = 0
    };
    
    int res = lzw_decompress(&decoder);

    // skip block end
    uint8_t last_byte;
    do {
        _read_byte(gif, &last_byte);    
    } while (last_byte != BLOCK_TERMINATOR);

    return res == 0;
}

void _process_gce(gif_handle_t gif, gif_gce_block_t *gce_buf)
{
    gif->active_gce.fields.u8 = gce_buf->fields;
    gif->active_gce.delay_time = gce_buf->delay_time;
    gif->active_gce.transparent_col_index = gce_buf->transparent_col_index;
    
    LOG("GCE: fields=[disp:%d;usr:%d;transp:%d]; delay=%d; transp_col_index=%d",
        gif->active_gce.fields.bits.disposal, gif->active_gce.fields.bits.user_input_flag, gif->active_gce.fields.bits.transparent_flag,
        gif->active_gce.delay_time, gif->active_gce.transparent_col_index);
    
    if (gif->first_gce_pos == 0) {
        gif->first_gce_pos = ftell(gif->f) - sizeof(gif_gce_block_t) - 3; // 3: sizeof(ext_type) + sizeof(block_sz) + 1
    }
}

bool _process_ext(gif_handle_t gif)
{
    uint8_t ext_type = 0;
    uint8_t block_sz = 0;
    uint8_t block_buf[255];

    _read_byte(gif, &ext_type);
    _read_byte(gif, &block_sz);
    _read_bytes(gif, &block_buf[0], block_sz);

    LOG("Extension type 0x%.02x; block_sz=%d", ext_type, block_sz);
    switch(ext_type) {
        case GRAPHIC_CONTROL:
            assert(block_sz == sizeof(gif_gce_block_t));
            gif_gce_block_t *gce = (gif_gce_block_t *)&block_buf[0];
            _process_gce(gif, gce);
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
                _read_byte(gif, &block_sz);
                _skip_bytes(gif, block_sz);
            } while (block_sz);
            break;
        case PLAIN_TEXT:
            assert(false && "Plain Text extension is not supported");
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
        _read_byte(gif, &block_type);
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
                LOG_ERR("Unrecognized block type %.02x @ 0x%.08lx", block_type, ftell(gif->f) - sizeof(block_type));
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
    fseek(gif->f, gif->first_gce_pos, SEEK_SET);
}

void minigif_deinit(gif_handle_t gif)
{
    _close_file(gif);
    free(gif);
}
