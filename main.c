#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "fbsplash.h"
#include "svg_parser.h"
#include "svg_renderer.h"
#include "dt_rotation.h"

/*
 * SVG path data for rendering the "ARCH R" logo
 * Generated from Quantico Bold font (https://fonts.google.com/specimen/Quantico)
 * Coordinate space: 1284 x 500
 *
 * Index meaning:
 * 0: "A" - blue
 * 1: "R" - blue
 * 2: "C" - blue
 * 3: "H" - blue
 * 4: "R" - white (separated by gap)
 */
const char *svg_paths[] = {
    /* A (Quantico Bold) */
    "M 180.9 130.0 L 266.6 370.0 L 219.3 370.0 L 201.1 318.2 L 111.3 318.2 L 93.1 370.0 L 45.8 370.0 L 131.5 130.0 Z M 155.8 190.3 L 125.7 277.1 L 186.3 277.1 L 156.5 190.3 Z",
    /* R (Quantico Bold) */
    "M 485.7 370.0 L 432.9 370.0 L 381.4 279.5 L 351.6 279.5 L 351.6 370.0 L 303.6 370.0 L 303.6 130.0 L 433.5 130.0 L 473.3 153.0 L 473.3 256.5 L 433.5 279.5 Z M 351.6 171.1 L 351.6 238.3 L 425.3 238.3 L 425.3 171.1 Z",
    /* C (Quantico Bold) */
    "M 662.9 299.7 L 710.9 299.7 L 710.9 348.7 L 671.1 371.7 L 574.1 371.7 L 534.3 348.7 L 534.3 151.3 L 574.1 128.3 L 671.1 128.3 L 710.9 151.3 L 710.9 200.3 L 662.9 200.3 L 662.9 169.4 L 582.3 169.4 L 582.3 330.6 L 662.9 330.6 Z",
    /* H (Quantico Bold) */
    "M 816.5 267.1 L 816.5 370.0 L 768.5 370.0 L 768.5 130.0 L 816.5 130.0 L 816.5 226.0 L 910.8 226.0 L 910.8 130.0 L 958.8 130.0 L 958.8 370.0 L 910.8 370.0 L 910.8 267.1 Z",
    /* R white (Quantico Bold) - separated by larger gap */
    "M 1226.6 370.0 L 1173.8 370.0 L 1122.3 279.5 L 1092.5 279.5 L 1092.5 370.0 L 1044.5 370.0 L 1044.5 130.0 L 1174.5 130.0 L 1214.2 153.0 L 1214.2 256.5 L 1174.5 279.5 Z M 1092.5 171.1 L 1092.5 238.3 L 1166.2 238.3 L 1166.2 171.1 Z"
};

/* Color definitions
 * "ARCH" = blue (#0088FF)
 * "R"    = white (#FFFFFF)
 */
const char *svg_colors[] = {
    "rgb(0,136,255)",   /* A - blue */
    "rgb(0,136,255)",   /* R - blue */
    "rgb(0,136,255)",   /* C - blue */
    "rgb(0,136,255)",   /* H - blue */
    "rgb(255,255,255)"  /* R - white */
};

/* Glow colors: dimmer versions for the subtle glow effect */
const char *glow_colors[] = {
    "rgb(0,48,112)",    /* A - dim blue glow */
    "rgb(0,48,112)",    /* R - dim blue glow */
    "rgb(0,48,112)",    /* C - dim blue glow */
    "rgb(0,48,112)",    /* H - dim blue glow */
    "rgb(80,80,112)"    /* R - dim gray-blue glow */
};

#define NUM_PATHS (sizeof(svg_paths) / sizeof(svg_paths[0]))

/* Apply a subtle glow by rendering the paths at small offsets with dim color */
static void render_glow(Framebuffer *fb, DisplayInfo *display_info, int rotation) {
    const float glow_offsets[][2] = {
        {-3, -3}, {-3, 0}, {-3, 3},
        { 0, -3},          { 0, 3},
        { 3, -3}, { 3, 0}, { 3, 3},
        {-2, -2}, {-2, 2}, { 2, -2}, { 2, 2},
        {-1, -1}, {-1, 1}, { 1, -1}, { 1, 1}
    };
    const int num_offsets = sizeof(glow_offsets) / sizeof(glow_offsets[0]);

    for (size_t i = 0; i < NUM_PATHS; i++) {
        for (int g = 0; g < num_offsets; g++) {
            SVGPath *svg = parse_svg_path(svg_paths[i], glow_colors[i]);
            if (!svg) continue;

            /* Offset all points for glow effect */
            for (uint32_t p = 0; p < svg->num_paths; p++) {
                for (uint32_t j = 0; j < svg->paths[p].num_points; j++) {
                    svg->paths[p].points[j].x += glow_offsets[g][0];
                    svg->paths[p].points[j].y += glow_offsets[g][1];
                }
            }

            if (rotation)
                rotate_svg_path(svg, rotation);

            render_svg_path(fb, svg, display_info);
            free_svg_path(svg);
        }
    }
}

/*
 * Main program entry point
 */
int main(void) {
    const char *fb_device = "/dev/fb0";

    /* Get rotation from device tree */
    int rotation = get_display_rotation();

    /* Check framebuffer device accessibility */
    if (access(fb_device, R_OK | W_OK) != 0) {
        fprintf(stderr, "Cannot access %s: %s\n", fb_device, strerror(errno));
        return 1;
    }

    /* Initialize framebuffer */
    Framebuffer *fb = fb_init(fb_device);
    if (!fb) {
        fprintf(stderr, "Failed to initialize framebuffer\n");
        return 1;
    }

    /* Calculate display parameters */
    DisplayInfo *display_info = calculate_display_info(fb);
    if (!display_info) {
        fprintf(stderr, "Failed to calculate display information\n");
        fb_cleanup(fb);
        return 1;
    }

    /* Clear screen to black */
    for (uint32_t y = 0; y < fb->vinfo.yres; y++) {
        for (uint32_t x = 0; x < fb->vinfo.xres; x++) {
            set_pixel(fb, x, y, 0x00000000);
        }
    }

    /* Render glow effect first (underneath the main logo) */
    render_glow(fb, display_info, rotation);

    /* Render main logo paths on top */
    for (size_t i = 0; i < NUM_PATHS; i++) {
        SVGPath *svg = parse_svg_path(svg_paths[i], svg_colors[i]);
        if (!svg) {
            fprintf(stderr, "Failed to parse SVG path %zu\n", i);
            continue;
        }

        if (rotation)
            rotate_svg_path(svg, rotation);

        render_svg_path(fb, svg, display_info);
        free_svg_path(svg);
    }

    /* Flush changes to the framebuffer */
    fb_flush(fb);

    /* Clean up */
    free(display_info);
    fb_cleanup(fb);

    return 0;
}
