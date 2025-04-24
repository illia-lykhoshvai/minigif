#include "minigif.h"

// debug start
#include "esp_log.h"
#define TAG __FILENAME__
#define MG_LOG_INFO(str, ...) ESP_LOGI(TAG, str, ##__VA_ARGS__)
#define MG_LOG_ERR(str, ...) ESP_LOGE(TAG, str, ##__VA_ARGS__)
// debug end

#define MAX(a,b) (a > b ? a : b)

static void _open_file(gif_handle_t gif, const char *name)
{
    gif->f = fopen(name, "rb");
    
    if (gif->f == NULL) {
        MG_LOG_ERR("fopen %s", name);
        return;
    }

    fseek(gif->f, 0, SEEK_SET);
}

static size_t _read_bytes(gif_handle_t gif, uint8_t *val, uint16_t size)
{
    return fread(val, sizeof(val[0]), size, gif->f);
}

static bool _check_gif_start(gif_handle_t gif)
{
    // check header and parse some fields
    const char correct_version[] = "GIF89a";
    gif_header_t header = {0};
    _read_bytes(gif, (uint8_t *)&header, sizeof(gif_header_t));
    
    if (0 != memcmp(correct_version, header.id_and_version, sizeof(header.id_and_version))) {
        MG_LOG_ERR("Wrong GIF header: %s, seek for %s", header.id_and_version, correct_version);
        return false;
    }

    bool global_color_table = (header.fields & 0x80) != 0;
    uint8_t color_resolution = ((header.fields & 0x70) >> 4) + 1;
    bool sort = (header.fields & 0x08) != 0;
    uint16_t global_color_table_size = 1 << ((header.fields & 0x07) + 1);

    MG_LOG_INFO("GIF fields: GCT:%d; CR=%d; SRT=%d; GCT_SZ=%d", global_color_table, color_resolution, sort, global_color_table_size);

    if (!global_color_table) {
        MG_LOG_ERR("GCT flag is not set...");
        return false;
    }
    
    gif->cr = color_resolution;
    gif->gct_size = global_color_table_size;

    // global color table, right after the header
    uint16_t gct_buf_size = sizeof(gif_rgb_t) * global_color_table_size;
    if (_read_bytes(gif, (uint8_t *)&gif->gct[0], gct_buf_size) != gct_buf_size) {
        MG_LOG_ERR("read GCT failed");
        return false;
    }

    return true;
}

gif_handle_t minigif_init(const char *filename, paint_cb_t paint_cb, void *user_data)
{
    assert(filename != NULL && paint_cb != NULL && "minigif_init params error");

    gif_handle_t gif = malloc(sizeof(gif_t));
    if (gif == NULL) {
        MG_LOG_ERR("malloc(sizeof(gif_t)) failed");
        return NULL;
    }

    gif->cb_user_data = user_data;
    gif->paint_cb = paint_cb;

    _open_file(gif, filename);

    if (!_check_gif_start(gif)) {
        free(gif);
        gif = NULL;
    }

    return gif;
}

bool _process_image_descriptor(gif_handle_t gif) 
{

    return true;
}

void minigif_render_frame(gif_handle_t gif)
{
    uint8_t block_type = 0;

    while (block_type != TRAILER) {
        if (_read_bytes(gif, &block_type, sizeof(block_type)) != sizeof(block_type)) {
            assert(true && "GIF file is corrupted!!!");
        }
        switch(block_type) {
            case IMAGE_DESCRIPTOR:
                if (!_process_image_descriptor(gif)) {
                    return;
                }
                break;
            case TRAILER:
                break;
            default:
                MG_LOG_ERR("Bailing on unrecognized block type %.02x", block_type);
                return;
        }
    }
}

static void _close_file(gif_handle_t gif)
{
    fclose(gif->f);
}

void minigif_deinit(gif_handle_t gif)
{
    _close_file(gif);
    free(gif);
}
