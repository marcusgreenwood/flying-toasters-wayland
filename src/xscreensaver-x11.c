/*
 * X11 path for xscreensaver - draws directly on XSCREENSAVER_WINDOW.
 * Uses raw Xlib to avoid SDL's BadWindow issues with xscreensaver's window.
 */
#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>
#include "../img/toast.xpm"
#include "../img/toaster.xpm"

#define TOASTER_SPRITE_COUNT 6
#define TOASTER_COUNT 6   /* Fewer sprites for Pi/X11 performance */
#define TOAST_COUNT 4
#define SPRITE_SIZE 64
#define GRID_WIDTH 4
#define GRID_HEIGHT 4
#define MAX_TOASTER_SPEED 4
#define MAX_TOAST_SPEED 3
#define FPS 60            /* One XPutImage per frame - fast enough now */

struct Toaster { int slot, x, y, moveDistance, currentFrame; };
struct Toast { int slot, x, y, moveDistance; };

static int hasSpriteCollision(int x1, int y1, int x2, int y2, int gap) {
    return (x1 < x2 + SPRITE_SIZE + gap) && (x2 < x1 + SPRITE_SIZE + gap) &&
           (y1 < y2 + SPRITE_SIZE + gap) && (y2 < y1 + SPRITE_SIZE + gap);
}
static int isScrolledToScreen(int x, int y, int w) {
    return (y + SPRITE_SIZE > 0) && (x + SPRITE_SIZE > 0) && (x < w);
}
static int isScrolledOutOfScreen(int x, int y, int h) {
    return (x <= -SPRITE_SIZE) || (y >= h);
}

static void setToasterSpawn(struct Toaster *t, int w, int h) {
    int sw = w / GRID_WIDTH, sh = h / GRID_HEIGHT;
    t->x = h + (t->slot % GRID_WIDTH) * sw + (sw - SPRITE_SIZE) / 2;
    t->y = -h + (t->slot / GRID_WIDTH) * sh + (sh - SPRITE_SIZE) / 2;
}
static void setToastSpawn(struct Toast *t, int w, int h) {
    int sw = w / GRID_WIDTH, sh = h / GRID_HEIGHT;
    t->x = h + (t->slot % GRID_WIDTH) * sw + (sw - SPRITE_SIZE) / 2;
    t->y = -h + (t->slot / GRID_WIDTH) * sh + (sh - SPRITE_SIZE) / 2;
}

static int *initGrid(void) {
    static int g[TOASTER_COUNT + TOAST_COUNT];
    for (int i = 0; i < TOASTER_COUNT + TOAST_COUNT; i++) g[i] = i;
    for (int i = 0; i < TOASTER_COUNT + TOAST_COUNT - 1; i++) {
        int j = i + rand() % (TOASTER_COUNT + TOAST_COUNT - i), t = g[j];
        g[j] = g[i]; g[i] = t;
    }
    return g;
}

static Window get_xscreensaver_window(Display *dpy) {
    (void)dpy;
    const char *s = getenv("XSCREENSAVER_WINDOW");
    if (!s || !*s) return 0;
    unsigned long id = 0;
    if (sscanf(s, " 0x%lx", &id) == 1 || sscanf(s, " %lu", &id) == 1 ||
        sscanf(s, "0x%lx", &id) == 1 || sscanf(s, "%lu", &id) == 1)
        return (Window)id;
    return 0;
}

/* Blit sprite onto buffer where mask is opaque. Uses XGetPixel/XPutPixel for format safety. */
static void blit_sprite(XImage *buf, XImage *sprite, XImage *mask, int dx, int dy, int buf_w, int buf_h) {
    for (int sy = 0; sy < SPRITE_SIZE; sy++) {
        int by = dy + sy;
        if (by < 0 || by >= buf_h) continue;
        for (int sx = 0; sx < SPRITE_SIZE; sx++) {
            int bx = dx + sx;
            if (bx < 0 || bx >= buf_w) continue;
            if (XGetPixel(mask, sx, sy) != 0)
                XPutPixel(buf, bx, by, XGetPixel(sprite, sx, sy));
        }
    }
}

static void draw_x11_composite(Display *dpy, Window win, XImage *bufImg,
    XImage **toasterImg, XImage **toasterMaskImg, XImage *toastImg, XImage *toastMaskImg,
    struct Toaster *toasters, struct Toast *toasts,
    int width, int height, unsigned long black)
{
    (void)dpy;
    (void)win;
    (void)black;
    /* Clear buffer (0 is typically black for TrueColor) */
    memset(bufImg->data, 0, (size_t)bufImg->bytes_per_line * height);

    for (int i = 0; i < TOAST_COUNT; i++) {
        if (isScrolledToScreen(toasts[i].x, toasts[i].y, width))
            blit_sprite(bufImg, toastImg, toastMaskImg, toasts[i].x, toasts[i].y, width, height);
    }
    for (int i = 0; i < TOASTER_COUNT; i++) {
        if (isScrolledToScreen(toasters[i].x, toasters[i].y, width)) {
            int f = toasters[i].currentFrame;
            blit_sprite(bufImg, toasterImg[f], toasterMaskImg[f], toasters[i].x, toasters[i].y, width, height);
        }
    }
}

int run_xscreensaver_x11(void) {
    const char *display_name = getenv("DISPLAY");
    if (!display_name || !*display_name) {
        display_name = ":0";
        setenv("DISPLAY", ":0", 1);
    }

    Display *dpy = XOpenDisplay(display_name);
    if (!dpy) {
        fprintf(stderr, "flying-toasters: cannot open display %s\n", display_name);
        return 1;
    }

    Window win = get_xscreensaver_window(dpy);
    if (!win) {
        fprintf(stderr, "flying-toasters: XSCREENSAVER_WINDOW not set or invalid\n");
        XCloseDisplay(dpy);
        return 1;
    }

    /* Don't XSelectInput - xscreensaver's window may deny attribute changes (BadAccess).
     * XScreensaver will kill our process when user activates. */

    XWindowAttributes xwa;
    if (!XGetWindowAttributes(dpy, win, &xwa)) {
        fprintf(stderr, "flying-toasters: cannot get window attributes\n");
        XCloseDisplay(dpy);
        return 1;
    }

    int width = xwa.width, height = xwa.height;
    Screen *screen = xwa.screen;
    Visual *vis = xwa.visual;
    int depth = xwa.depth;

    unsigned long black = BlackPixelOfScreen(screen);
    GC gc = XCreateGC(dpy, win, 0, NULL);

    XImage *toasterImg[TOASTER_SPRITE_COUNT];
    XImage *toasterMaskImg[TOASTER_SPRITE_COUNT];
    XImage *clipMask = NULL;

    for (int i = 0; i < TOASTER_SPRITE_COUNT; i++) {
        if (XpmCreateImageFromData(dpy, toasterXpm[i], &toasterImg[i], &clipMask, NULL) != 0) {
            fprintf(stderr, "flying-toasters: failed to load toaster sprite %d\n", i);
            for (int j = 0; j < i; j++) {
                XDestroyImage(toasterImg[j]);
                XDestroyImage(toasterMaskImg[j]);
            }
            XFreeGC(dpy, gc);
            XCloseDisplay(dpy);
            return 1;
        }
        toasterMaskImg[i] = clipMask;
        clipMask = NULL;
    }

    XImage *toastImg = NULL;
    if (XpmCreateImageFromData(dpy, toastXpm, &toastImg, &clipMask, NULL) != 0) {
        fprintf(stderr, "flying-toasters: failed to load toast sprite\n");
        for (int i = 0; i < TOASTER_SPRITE_COUNT; i++) {
            XDestroyImage(toasterImg[i]);
            XDestroyImage(toasterMaskImg[i]);
        }
        XFreeGC(dpy, gc);
        XCloseDisplay(dpy);
        return 1;
    }
    XImage *toastMaskImg = clipMask;

    /* Screen buffer - one XPutImage per frame instead of many with clip masks */
    XImage *bufImg = XCreateImage(dpy, vis, (unsigned)depth, ZPixmap, 0, NULL,
        (unsigned)width, (unsigned)height, 32, 0);
    if (!bufImg) {
        fprintf(stderr, "flying-toasters: XCreateImage failed\n");
        for (int i = 0; i < TOASTER_SPRITE_COUNT; i++) {
            XDestroyImage(toasterImg[i]);
            XDestroyImage(toasterMaskImg[i]);
        }
        XDestroyImage(toastImg);
        XDestroyImage(toastMaskImg);
        XFreeGC(dpy, gc);
        XCloseDisplay(dpy);
        return 1;
    }
    bufImg->data = (char *)calloc(1, (size_t)bufImg->bytes_per_line * height);
    if (!bufImg->data) {
        XDestroyImage(bufImg);
        for (int i = 0; i < TOASTER_SPRITE_COUNT; i++) {
            XDestroyImage(toasterImg[i]);
            XDestroyImage(toasterMaskImg[i]);
        }
        XDestroyImage(toastImg);
        XDestroyImage(toastMaskImg);
        XFreeGC(dpy, gc);
        XCloseDisplay(dpy);
        return 1;
    }

    int *grid = initGrid();
    struct Toaster toasters[TOASTER_COUNT];
    struct Toast toasts[TOAST_COUNT];

    for (int i = 0; i < TOASTER_COUNT; i++) {
        toasters[i].slot = grid[i];
        toasters[i].moveDistance = 1 + rand() % MAX_TOASTER_SPEED;
        toasters[i].currentFrame = rand() % TOASTER_SPRITE_COUNT;
        setToasterSpawn(&toasters[i], width, height);
    }
    for (int i = 0; i < TOAST_COUNT; i++) {
        toasts[i].slot = grid[TOASTER_COUNT + i];
        toasts[i].moveDistance = 1 + rand() % MAX_TOAST_SPEED;
        setToastSpawn(&toasts[i], width, height);
    }

    int frame = 0;
    while (1) {
        for (int i = 0; i < TOAST_COUNT; i++) {
            int nx = toasts[i].x - toasts[i].moveDistance;
            int ny = toasts[i].y + toasts[i].moveDistance;
            if (isScrolledOutOfScreen(nx, ny, height))
                setToastSpawn(&toasts[i], width, height);
            else { toasts[i].x = nx; toasts[i].y = ny; }
        }

        for (int i = 0; i < TOASTER_COUNT; i++) {
            int nx = toasters[i].x - toasters[i].moveDistance;
            int ny = toasters[i].y + toasters[i].moveDistance;
            if (isScrolledOutOfScreen(nx, ny, height))
                setToasterSpawn(&toasters[i], width, height);
            else {
                for (int j = 0; j < TOASTER_COUNT; j++) {
                    if (i != j && hasSpriteCollision(toasters[j].x, toasters[j].y, nx, ny, 0)) {
                        if (toasters[i].x <= toasters[j].x + SPRITE_SIZE) ny = toasters[i].y + toasters[j].moveDistance;
                        else nx = toasters[i].x - toasters[j].moveDistance;
                        break;
                    }
                }
                toasters[i].x = nx; toasters[i].y = ny;
            }
            if (frame % (10 - toasters[i].moveDistance) == 0)
                toasters[i].currentFrame = (toasters[i].currentFrame + 1) % TOASTER_SPRITE_COUNT;
        }
        frame = (frame + 1) % 256;

        draw_x11_composite(dpy, win, bufImg, toasterImg, toasterMaskImg, toastImg, toastMaskImg,
            toasters, toasts, width, height, black);
        XPutImage(dpy, win, gc, bufImg, 0, 0, 0, 0, width, height);
        XFlush(dpy);

        { struct timespec ts = { 0, (long)(1000000000 / FPS) }; nanosleep(&ts, NULL); }
    }

    free(bufImg->data);
    XDestroyImage(bufImg);
    for (int i = 0; i < TOASTER_SPRITE_COUNT; i++) {
        XDestroyImage(toasterImg[i]);
        XDestroyImage(toasterMaskImg[i]);
    }
    XDestroyImage(toastImg);
    XDestroyImage(toastMaskImg);
    XFreeGC(dpy, gc);
    XCloseDisplay(dpy);
    return 0;
}
