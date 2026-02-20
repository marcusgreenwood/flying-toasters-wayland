#include "xpm.h"
#include <stdlib.h>
#include <string.h>

#define MAX_COLORS 64

typedef struct {
    char key[8];
    unsigned char r, g, b, a;
} ColorEntry;

static int parse_hex(const char *s) {
    int r = 0;
    if (s[0] == '#') s++;
    while (*s) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else break;
        r = (r << 4) | d;
        s++;
    }
    return r;
}

static int parse_color(const char *line, int cpp, ColorEntry *out) {
    const char *p = line;

    /* Extract key (cpp chars) */
    int i;
    for (i = 0; i < cpp && i < 7; i++) {
        out->key[i] = *p++;
    }
    out->key[i] = '\0';

    /* Skip to 'c' or 'g' */
    while (*p && *p != ' ' && *p != '\t') p++;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (*p != 'c' && *p != 'g') return -1;
    p++;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (!*p) return -1;

    /* Parse color value */
    if (strncmp(p, "None", 4) == 0) {
        out->r = out->g = out->b = 0;
        out->a = 0;
        return 0;
    }
    if (*p == '#') {
        int hex = parse_hex(p);
        out->r = (hex >> 16) & 0xff;
        out->g = (hex >> 8) & 0xff;
        out->b = hex & 0xff;
        out->a = 255;
        return 0;
    }
    return -1;
}

static ColorEntry *find_color(ColorEntry *colors, int n, const char *key, int cpp) {
    char k[8];
    int i;
    for (i = 0; i < cpp && i < 7; i++) k[i] = key[i];
    k[i] = '\0';
    for (i = 0; i < n; i++) {
        if (strcmp(colors[i].key, k) == 0) return &colors[i];
    }
    return NULL;
}

SDL_Surface *xpm_to_surface(const char *const *xpm_data) {
    int width, height, ncolors, cpp;
    if (sscanf(xpm_data[0], "%d %d %d %d", &width, &height, &ncolors, &cpp) != 4)
        return NULL;
    if (width <= 0 || height <= 0 || ncolors <= 0 || ncolors > MAX_COLORS || cpp <= 0 || cpp > 4)
        return NULL;

    ColorEntry colors[MAX_COLORS];
    for (int i = 0; i < ncolors; i++) {
        if (parse_color(xpm_data[1 + i], cpp, &colors[i]) != 0)
            return NULL;
    }

    int pitch = width * 4;
    Uint32 *pixels = (Uint32 *)malloc((size_t)pitch * height);
    if (!pixels) return NULL;

    for (int y = 0; y < height; y++) {
        const char *row = xpm_data[1 + ncolors + y];
        if (!row) { free(pixels); return NULL; }
        for (int x = 0; x < width; x++) {
            ColorEntry *c = find_color(colors, ncolors, row + x * cpp, cpp);
            if (!c) { free(pixels); return NULL; }
            Uint32 px = (c->a << 24) | (c->r << 16) | (c->g << 8) | c->b;
            pixels[y * width + x] = px;
        }
    }

    SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(
        pixels, width, height, 32, pitch,
        0xff, 0xff00, 0xff0000, 0xff000000
    );
    if (!surf) { free(pixels); return NULL; }

    /* Use SDL's internal pixel ownership - we need to NOT free pixels when freeing surface.
     * Actually SDL_CreateRGBSurfaceFrom does NOT take ownership - we must free pixels.
     * But if we free surf, SDL doesn't free the pixels. So we need to free pixels when
     * we free the surface. The problem: SDL_FreeSurface won't free our buffer.
     * We need to use a custom approach - create surface with SDL_PIXELFORMAT_RGBA8888
     * and copy our pixels, then free our buffer. Or use SDL_CreateRGBSurface and copy.
     */
    SDL_Surface *final = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(surf);
    free(pixels);
    if (!final) return NULL;

    SDL_SetSurfaceBlendMode(final, SDL_BLENDMODE_BLEND);
    return final;
}
