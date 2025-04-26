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

typedef void (*painter_cb_t)(uint16_t x, uint16_t y, gif_rgb_t color, void *user_data);

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
} gif_gce_t;

typedef struct {
    FILE *f;

    uint16_t width, height;
    
    painter_cb_t painter;
    void *user_data; // passed to callback

    uint8_t cr; // color resolution, parsed from header.fields
    
    gif_rgb_t gct[256]; // Global Color Table
    uint16_t gct_size;
    
    gif_rgb_t lct[256]; // Local Color Table
    uint16_t lct_size;
    
    int first_gce_pos;
    gif_gce_t active_gce; // Graphic Active Control
} gif_t;

typedef gif_t *gif_handle_t;

typedef enum {
    GIF_STATUS_ERROR,
    GIF_STATUS_FRAME_END,
    GIF_STATUS_GIF_END,
} gif_status_t;

gif_handle_t minigif_init(const char *filename, painter_cb_t paint_cb, void *user_data);
void minigif_deinit(gif_handle_t gif);

gif_status_t minigif_render_frame(gif_handle_t gif);
uint32_t minigif_get_frame_delay(gif_handle_t gif);
void minigif_rewind(gif_handle_t gif);

#endif // ifndef MINIGIF_H
