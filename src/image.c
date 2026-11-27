#include "batch.h"

#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>

static int clamp(int value, int max) {
    if (value < 0)
        return 0;
    return value < max ? value : max;
}

static int source_coordinate(int position, int from, int to) {
    /* Use the Studio's pixel centres and nearest-integer rule. */
    float centre = (position + 0.5f) * from / to - 0.5f;
    return clamp((int)lroundf(centre), from - 1);
}

static void resize_and_brighten(const unsigned char *source, int w, int h,
                               unsigned char *resized, Recipe recipe) {
    int columns[10000];
    for (int x = 0; x < recipe.width; ++x)
        columns[x] = 3 * source_coordinate(x, w, recipe.width);
    for (int y = 0; y < recipe.height; ++y) {
        int sy = source_coordinate(y, h, recipe.height);
        const unsigned char *row = source + (size_t)sy * w * 3;
        for (int x = 0; x < recipe.width; ++x) {
            for (int c = 0; c < 3; ++c)
                *resized++ = (unsigned char)clamp(row[columns[x] + c] + recipe.brightness, 255);
        }
    }
}

static void box_blur(const unsigned char *source, unsigned char *filtered, int w, int h) {
    for (int y = 0; y < h; ++y) {
        const unsigned char *top = source + (size_t)clamp(y - 1, h - 1) * w * 3;
        const unsigned char *middle = source + (size_t)y * w * 3;
        const unsigned char *bottom = source + (size_t)clamp(y + 1, h - 1) * w * 3;
        for (int x = 0; x < w; ++x) {
            int left = 3 * clamp(x - 1, w - 1), centre = 3 * x, right = 3 * clamp(x + 1, w - 1);
            for (int c = 0; c < 3; ++c) {
                int sum = top[left + c] + top[centre + c] + top[right + c] +
                          middle[left + c] + middle[centre + c] + middle[right + c] +
                          bottom[left + c] + bottom[centre + c] + bottom[right + c];
                *filtered++ = (unsigned char)((sum + 4) / 9);
            }
        }
    }
}

static int save_image(const char *output, int id, const unsigned char *pixels, Recipe recipe) {
    char final[PATH_MAX], temporary[PATH_MAX];
    snprintf(final, sizeof final, "%s/job-%05d.png", output, id);
    snprintf(temporary, sizeof temporary, "%s/.job-%05d.tmp", output, id);

    png_image image = {0};
    image.version = PNG_IMAGE_VERSION;
    image.width = (png_uint_32)recipe.width;
    image.height = (png_uint_32)recipe.height;
    image.format = PNG_FORMAT_RGB;
    int written = png_image_write_to_file(&image, temporary, 0, pixels, 0, NULL);
    png_image_free(&image);
    if (written && rename(temporary, final) == 0)
        return SAVED;
    remove(temporary);
    return SAVE_FAILED;
}

int process_image(const char *input, const char *output, int id, Recipe recipe) {
    png_image image = {0};
    image.version = PNG_IMAGE_VERSION;
    unsigned char *source = NULL, *resized = NULL, *filtered = NULL;
    int status = LOAD_FAILED;
    if (!png_image_begin_read_from_file(&image, input) || !image.width || !image.height ||
        image.width > 10000 || image.height > 10000 || image.width * image.height > 10000000)
        goto cleanup;
    int w = (int)image.width, h = (int)image.height;
    image.format = PNG_FORMAT_RGB;
    source = malloc(PNG_IMAGE_SIZE(image));
    size_t bytes = (size_t)recipe.width * recipe.height * 3;
    resized = malloc(bytes);
    filtered = malloc(bytes);
    status = ALLOC_FAILED;
    if (!source || !resized || !filtered)
        goto cleanup;
    status = LOAD_FAILED;
    const png_color black = {0, 0, 0};
    if (!png_image_finish_read(&image, &black, source, 0, NULL))
        goto cleanup;
    resize_and_brighten(source, w, h, resized, recipe);
    box_blur(resized, filtered, recipe.width, recipe.height);
    status = save_image(output, id, filtered, recipe);

cleanup:
    png_image_free(&image);
    free(source);
    free(resized);
    free(filtered);
    return status;
}
