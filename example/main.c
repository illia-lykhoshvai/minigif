#include "raylib.h"
#include "minigif.h"
#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "unistd.h"
#include "time.h"

static long _read_bytes_cb(void *f, uint8_t *val, size_t size)
{
    FILE *file = (FILE *)f;
    size_t read_amount = fread(val, sizeof(val[0]), size, file);
    assert(size == read_amount);
    return read_amount;
}

static long _lseek_cb(void *f, long off, int seek)
{
    FILE *file = (FILE *)f;
    if ((off == 0) && (seek == SEEK_CUR)) {
        return ftell(file);
    }
    return fseek(file, off, seek);
}

static void _painter_cb(uint16_t x, uint16_t y, gif_rgb_t rgb, void *user_data) {
    Image *img = (Image *)user_data;

    Color color = {
        .r = rgb.r,
        .g = rgb.g,
        .b = rgb.b,
        .a = 255
    };

    ImageDrawPixel(img, x, y, color);
};

static uint64_t get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

int main(void)
{
    Image image; // empty image to draw into

    uint8_t gif_buf[sizeof(gif_t)];

    FILE *f = fopen("./test.gif", "rb");
    assert(f != NULL);

    gif_cb_t gif_cb = {
        .read = _read_bytes_cb,
        .lseek = _lseek_cb,
        .painter = _painter_cb,
        .user_data = &image,
    };

    gif_handle_t gif = minigif_init(gif_buf, (void *)f, gif_cb);
    assert(gif != NULL);

    image = GenImageColor(gif->width, gif->height, BLANK); // RGBA 32-bit image

    InitWindow(gif->width, gif->height, "minigif example");

    while (!WindowShouldClose())
    {
        uint64_t t0 = get_time_ms();

        gif_status_t ret = minigif_render_frame(gif);
        assert(ret != GIF_STATUS_ERROR);

        if (ret == GIF_STATUS_GIF_END) {
            minigif_rewind(gif);
            minigif_render_frame(gif);
        }

        BeginDrawing();
            DrawTexture(LoadTextureFromImage(image), 0, 0, WHITE);
        EndDrawing();

        uint64_t t1 = get_time_ms();
        uint32_t delta = t1 - t0;
        printf("gif drawing took: %u ms\n", delta);
        uint32_t gif_delay_ms = minigif_get_frame_delay(gif) * 10;
        uint32_t task_delay_ms = gif_delay_ms > delta ? gif_delay_ms - delta : 1;
        usleep(task_delay_ms * 1000); // microseconds
    }

    UnloadImage(image);
    fclose(f);
    CloseWindow();
    return 0;
}
