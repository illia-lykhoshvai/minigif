#ifndef MINIGIF_H
#define MINIGIF_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "minigif_structs.h"

typedef long (*read_cb_t)(void *f, uint8_t *val, size_t size);
typedef long (*lseek_cb_t)(void *f, long off, int seek);
typedef void (*painter_cb_t)(uint16_t x, uint16_t y, gif_rgb_t color, void *user_data);

typedef struct {
    read_cb_t read;
    lseek_cb_t lseek;
    painter_cb_t painter;
    void *user_data; // passed only to painter
} gif_cb_t;

typedef struct {
    void *f;
    
    gif_cb_t cb;
    
    uint16_t width, height;
    uint8_t bg_index;

    gif_rgb_t gct[256]; // Global Color Table
    uint16_t gct_size;
    
    gif_rgb_t lct[256]; // Local Color Table
    uint16_t lct_size;
    
    uint32_t first_gce_pos;
    gif_gce_t active_gce; // active Graphic Control Extension blocks
    
    gif_img_descr_t img_descr; // active Image Descriptor block

    lzw_context_t lzw_ctx;
} gif_t;

typedef gif_t *gif_handle_t;

typedef enum {
    GIF_STATUS_ERROR,
    GIF_STATUS_FRAME_END,
    GIF_STATUS_GIF_END,
} gif_status_t;

gif_handle_t minigif_init(uint8_t buffer[sizeof(gif_t)], void *f, gif_cb_t callbacks);

gif_status_t minigif_render_frame(gif_handle_t gif);
uint32_t minigif_get_frame_delay(gif_handle_t gif);
void minigif_rewind(gif_handle_t gif);

#endif // ifndef MINIGIF_H
