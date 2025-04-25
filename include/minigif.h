#ifndef MINIGIF_H
#define MINIGIF_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} gif_rgb_t;

typedef void (*paint_cb_t)(uint16_t x, uint16_t y, gif_rgb_t color, void *user_data);

typedef struct {
    FILE *f;
    
    void *cb_user_data;
    paint_cb_t paint_cb;

    uint8_t cr; // color resolution, parsed from header.fields
    bool gct_active; // true: use GCT; false: use LCT
    
    gif_rgb_t gct[256]; // Global Color Table
    uint16_t gct_size;
    
    gif_rgb_t lct[256]; // Local Color Table
    uint16_t lct_size;
    
    uint32_t frame_delay;
    uint32_t next_delay;
    
    int first_gce;
} gif_t;

typedef gif_t *gif_handle_t;

typedef enum {
    GIF_STATUS_ERROR,
    GIF_STATUS_FRAME_END,
    GIF_STATUS_GIF_END,
} gif_status_t;

gif_handle_t minigif_init(const char *filename, paint_cb_t paint_cb, void *user_data);
void minigif_deinit(gif_handle_t gif);

gif_status_t minigif_render_frame(gif_handle_t gif);
uint32_t minigif_get_frame_delay(gif_handle_t gif);
void minigif_rewind(gif_handle_t gif);

#endif // ifndef MINIGIF_H
