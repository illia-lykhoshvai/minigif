#ifndef MINIGIF_H
#define MINIGIF_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

typedef enum {
    IMAGE_DESCRIPTOR = 0x2C,
    TRAILER = 0x3B,
} block_id_t;

typedef struct {
    char id_and_version[6];
    uint16_t width;
    uint16_t height;
    uint8_t fields;
    uint8_t bg_color_index;
    uint8_t pixel_aspect_ratio;
} __packed gif_header_t;

typedef struct {
    uint16_t x0;
    uint16_t y0;
    uint16_t w;
    uint16_t h;
    uint8_t fields;
} __packed gif_img_block_header_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} __packed gif_rgb_t;

typedef void (*paint_cb_t)(uint16_t x, uint16_t y, uint16_t color, void *user_data);

typedef struct {
    FILE *f;
    
    void *cb_user_data;
    paint_cb_t paint_cb;

    uint8_t cr; // color resolution, parsed from header.fields
    
    gif_rgb_t gct[256];// Global Color Table
    uint16_t gct_size;
} gif_t;

typedef gif_t *gif_handle_t;

gif_handle_t minigif_init(const char *filename, paint_cb_t paint_cb, void *user_data);
void minigif_deinit(gif_handle_t gif);

void minigif_render_frame(gif_handle_t gif);

#endif // ifndef MINIGIF_H
