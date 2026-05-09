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
 * SVG path data for rendering the "NextOS" logo
 * Generated from Quantico Bold font (https://fonts.google.com/specimen/Quantico)
 * Coordinate space: 1284 x 500
 *
 * Index meaning:
 * 0: "N" - blue
 * 1: "e" - blue
 * 2: "x" - blue
 * 3: "t" - blue
 * 4: "O" - white (separated by gap)
 * 5: "S" - white
 */
const char *svg_paths[] = {
    /* N (Quantico Bold) */
    "M 100.8 225.0V 370.0H 56.0V 146.0H 97.9L 194.6 291.0H 195.2V 146.0H 240.0V 370.0H 198.1L 101.4 225.0Z",
    /* e (Quantico Bold) */
    "M 424.0 332.6V 370.0H 328.3L 291.2 348.6V 229.8L 328.3 208.4H 386.9L 424.0 229.8V 306.3H 335.0V 332.6ZM 335.0 245.8V 273.4H 380.2V 245.8Z",
    /* x (Quantico Bold) */
    "M 452.8 370.0 L 511.0 288.4 L 454.7 210.0H 502.4L 533.8 255.1L 565.1 210.0H 612.8L 556.2 288.4L 614.7 370.0H 567.7L 533.8 322.0L 499.8 370.0Z",
    /* t (Quantico Bold) */
    "M 630.7 247.4V 210.0H 660.2V 183.1L 704.0 157.8V 210.0H 739.2V 247.4H 704.0V 332.6H 739.2V 370.0H 697.3L 660.2 348.6V 247.4Z",
    /* O white (Quantico Bold) - separated by larger gap */
    "M 1018.4 350.2 L 981.3 371.6H 881.1L 844.0 350.2V 165.8L 881.1 144.4H 981.3L 1018.4 165.8ZM 888.8 182.8V 333.2H 973.6V 182.8Z",
    /* S (Quantico Bold) */
    "M 1072.8 256.4V 165.8L 1109.9 144.4H 1190.9L 1228.0 165.8V 208.4H 1183.2V 182.8H 1117.6V 236.2H 1190.9L 1228.0 257.7V 350.2L 1190.9 371.6H 1106.7L 1069.6 350.2V 307.6H 1114.4V 333.2H 1183.2V 277.8H 1109.9Z"
};

/* Color definitions
 * "Next" = blue (#0088FF)
 * "OS"   = white (#FFFFFF)
 */
const char *svg_colors[] = {
    "rgb(0,136,255)",   /* N - blue */
    "rgb(0,136,255)",   /* e - blue */
    "rgb(0,136,255)",   /* x - blue */
    "rgb(0,136,255)",   /* t - blue */
    "rgb(255,255,255)", /* O - white */
    "rgb(255,255,255)"  /* S - white */
};

/* Glow colors: dimmer versions for the subtle glow effect */
const char *glow_colors[] = {
    "rgb(0,48,112)",    /* N - dim blue glow */
    "rgb(0,48,112)",    /* e - dim blue glow */
    "rgb(0,48,112)",    /* x - dim blue glow */
    "rgb(0,48,112)",    /* t - dim blue glow */
    "rgb(80,80,112)",   /* O - dim gray-blue glow */
    "rgb(80,80,112)"    /* S - dim gray-blue glow */
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
