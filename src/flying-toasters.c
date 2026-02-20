#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#include <stdio.h>
#include "../img/toast.xpm"
#include "../img/toaster.xpm"
#include "xpm.h"
#include "flying-toasters.h"

#define TOASTER_SPRITE_COUNT 6
#define TOASTER_COUNT 10
#define TOAST_COUNT 6
#define SPRITE_SIZE 64
#define GRID_WIDTH 4
#define GRID_HEIGHT 4
#define MAX_TOASTER_SPEED 4
#define MAX_TOAST_SPEED 3
#define FPS 60

/* Parse XSCREENSAVER_WINDOW env var (hex "0x123" or decimal "123") */
static int get_xscreensaver_window(unsigned long *out) {
    const char *s = getenv("XSCREENSAVER_WINDOW");
    if (!s || !*s) return 0;
    if (sscanf(s, " 0x%lx", out) == 1) return 1;
    if (sscanf(s, " %lu", out) == 1) return 1;
    if (sscanf(s, "0x%lx", out) == 1) return 1;
    if (sscanf(s, "%lu", out) == 1) return 1;
    return 0;
}

int main(int argc, char *argv[]) {
    srand((unsigned)time(NULL));

    int windowed = (argc > 1 && strcmp(argv[1], "-windowed") == 0);
    unsigned long xss_win = 0;
    int use_xscreensaver = get_xscreensaver_window(&xss_win);

    /* When run by xscreensaver, we must use X11 and draw on its window */
    if (use_xscreensaver) {
        setenv("SDL_VIDEODRIVER", "x11", 1);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window;
    if (use_xscreensaver) {
        window = SDL_CreateWindowFrom((void *)(unsigned long)xss_win);
        if (!window) {
            fprintf(stderr, "SDL_CreateWindowFrom(XSCREENSAVER_WINDOW) failed: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
    } else {
        Uint32 win_flags = SDL_WINDOW_SHOWN;
        int win_w = 1920, win_h = 1080;
        if (!windowed) {
            win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
                win_w = dm.w;
                win_h = dm.h;
            }
        }
        window = SDL_CreateWindow(
            "Flying Toasters",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            win_w, win_h,
            win_flags
        );
        if (!window) {
            fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
    }

#ifdef __linux__
    /* Software renderer avoids GPU/Wayland flickering - except when embedding in xscreensaver */
    if (!use_xscreensaver)
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
#endif

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    }
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, 0);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        if (!use_xscreensaver) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *toasterTextures[TOASTER_SPRITE_COUNT];
    SDL_Texture *toastTexture = NULL;
    loadSprites(renderer, toasterTextures, &toastTexture);
    if (!toastTexture) {
        fprintf(stderr, "Failed to load sprites\n");
        SDL_DestroyRenderer(renderer);
        if (!use_xscreensaver) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

#ifdef __linux__
    /* Let Wayland compositor finish window setup before drawing (not needed when embedded in xscreensaver) */
    if (!use_xscreensaver)
        SDL_Delay(200);
#endif

    struct Toaster toasters[TOASTER_COUNT];
    struct Toast toasts[TOAST_COUNT];
    int *grid = initGrid();

    spawnToasters(toasters, width, height, grid);
    spawnToasts(toasts, width, height, grid);

    int frameCounter = 0;
    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
                running = 0;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        frameCounter = (frameCounter + 1) % 256;

        /* Draw and update toasts */
        for (int i = 0; i < TOAST_COUNT; i++) {
            if (isScrolledToScreen(toasts[i].x, toasts[i].y, width)) {
                drawSprite(renderer, toastTexture, toasts[i].x, toasts[i].y);
            }
            int newX = toasts[i].x - toasts[i].moveDistance;
            int newY = toasts[i].y + toasts[i].moveDistance;
            if (isScrolledOutOfScreen(newX, newY, height)) {
                setToastSpawnCoordinates(&toasts[i], width, height);
            } else {
                toasts[i].x = newX;
                toasts[i].y = newY;
            }
        }

        /* Draw and update toasters */
        for (int i = 0; i < TOASTER_COUNT; i++) {
            if (isScrolledToScreen(toasters[i].x, toasters[i].y, width)) {
                drawSprite(renderer, toasterTextures[toasters[i].currentFrame],
                          toasters[i].x, toasters[i].y);
            }
            int newX = toasters[i].x - toasters[i].moveDistance;
            int newY = toasters[i].y + toasters[i].moveDistance;
            if (isScrolledOutOfScreen(newX, newY, height)) {
                setToasterSpawnCoordinates(&toasters[i], width, height);
            } else {
                for (int j = 0; j < TOASTER_COUNT; j++) {
                    if (i != j && hasSpriteCollision(toasters[j].x, toasters[j].y, newX, newY, 0)) {
                        if (toasters[i].x <= toasters[j].x + SPRITE_SIZE) {
                            newY = toasters[i].y + toasters[j].moveDistance;
                        } else {
                            newX = toasters[i].x - toasters[j].moveDistance;
                        }
                        break;
                    }
                }
                toasters[i].x = newX;
                toasters[i].y = newY;
            }
            if (frameCounter % (10 - toasters[i].moveDistance) == 0) {
                toasters[i].currentFrame = (toasters[i].currentFrame + 1) % TOASTER_SPRITE_COUNT;
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(1000 / FPS);
    }

    freeSprites(toasterTextures, toastTexture);
    SDL_DestroyRenderer(renderer);
    if (!use_xscreensaver)
        SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

int hasSpriteCollision(int x1, int y1, int x2, int y2, int gap) {
    return (x1 < x2 + SPRITE_SIZE + gap) && (x2 < x1 + SPRITE_SIZE + gap) &&
           (y1 < y2 + SPRITE_SIZE + gap) && (y2 < y1 + SPRITE_SIZE + gap);
}

int isScrolledToScreen(int x, int y, int screenWidth) {
    return (y + SPRITE_SIZE > 0) && (x + SPRITE_SIZE > 0) && (x < screenWidth);
}

int isScrolledOutOfScreen(int x, int y, int screenHeight) {
    return (x <= -SPRITE_SIZE) || (y >= screenHeight);
}

void loadSprites(SDL_Renderer *renderer,
                 SDL_Texture **toasterTextures,
                 SDL_Texture **toastTexture) {
    for (int i = 0; i < TOASTER_SPRITE_COUNT; i++) {
        SDL_Surface *surf = xpm_to_surface((const char *const *)toasterXpm[i]);
        if (!surf) {
            for (int j = 0; j < i; j++) SDL_DestroyTexture(toasterTextures[j]);
            *toastTexture = NULL;
            return;
        }
        toasterTextures[i] = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);
    }
    SDL_Surface *toastSurf = xpm_to_surface((const char *const *)toastXpm);
    if (!toastSurf) {
        for (int i = 0; i < TOASTER_SPRITE_COUNT; i++) SDL_DestroyTexture(toasterTextures[i]);
        *toastTexture = NULL;
        return;
    }
    *toastTexture = SDL_CreateTextureFromSurface(renderer, toastSurf);
    SDL_FreeSurface(toastSurf);
}

void freeSprites(SDL_Texture **toasterTextures, SDL_Texture *toastTexture) {
    for (int i = 0; i < TOASTER_SPRITE_COUNT; i++) {
        SDL_DestroyTexture(toasterTextures[i]);
    }
    SDL_DestroyTexture(toastTexture);
}

void setToasterSpawnCoordinates(struct Toaster *toaster, int screenWidth, int screenHeight) {
    int slotWidth = screenWidth / GRID_WIDTH;
    int slotHeight = screenHeight / GRID_HEIGHT;
    toaster->x = screenHeight + (toaster->slot % GRID_WIDTH) * slotWidth + (slotWidth - SPRITE_SIZE) / 2;
    toaster->y = -screenHeight + (toaster->slot / GRID_WIDTH) * slotHeight + (slotHeight - SPRITE_SIZE) / 2;
}

void setToastSpawnCoordinates(struct Toast *toast, int screenWidth, int screenHeight) {
    int slotWidth = screenWidth / GRID_WIDTH;
    int slotHeight = screenHeight / GRID_HEIGHT;
    toast->x = screenHeight + (toast->slot % GRID_WIDTH) * slotWidth + (slotWidth - SPRITE_SIZE) / 2;
    toast->y = -screenHeight + (toast->slot / GRID_WIDTH) * slotHeight + (slotHeight - SPRITE_SIZE) / 2;
}

void spawnToasters(struct Toaster *toasters, int screenWidth, int screenHeight, int *grid) {
    for (int i = 0; i < TOASTER_COUNT; i++) {
        toasters[i].slot = grid[i];
        toasters[i].moveDistance = 1 + rand() % MAX_TOASTER_SPEED;
        toasters[i].currentFrame = rand() % TOASTER_SPRITE_COUNT;
        setToasterSpawnCoordinates(&toasters[i], screenWidth, screenHeight);
    }
}

void spawnToasts(struct Toast *toasts, int screenWidth, int screenHeight, int *grid) {
    for (int i = 0; i < TOAST_COUNT; i++) {
        toasts[i].slot = grid[TOASTER_COUNT + i];
        toasts[i].moveDistance = 1 + rand() % MAX_TOAST_SPEED;
        setToastSpawnCoordinates(&toasts[i], screenWidth, screenHeight);
    }
}

void drawSprite(SDL_Renderer *renderer, SDL_Texture *texture, int x, int y) {
    if (!texture) return;
    SDL_Rect dst = { x, y, SPRITE_SIZE, SPRITE_SIZE };
    SDL_RenderCopy(renderer, texture, NULL, &dst);
}

int *initGrid(void) {
    static int grid[TOASTER_COUNT + TOAST_COUNT];
    for (int i = 0; i < TOASTER_COUNT + TOAST_COUNT; i++) {
        grid[i] = i;
    }
    for (int i = 0; i < TOASTER_COUNT + TOAST_COUNT - 1; i++) {
        int j = i + rand() % (TOASTER_COUNT + TOAST_COUNT - i);
        int t = grid[j];
        grid[j] = grid[i];
        grid[i] = t;
    }
    return grid;
}
