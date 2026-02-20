#ifndef FLYING_TOASTERS_H
#define FLYING_TOASTERS_H

#include <SDL.h>

struct Toaster {
    int slot;
    int x;
    int y;
    int moveDistance;
    int currentFrame;
};

struct Toast {
    int slot;
    int x;
    int y;
    int moveDistance;
};

int hasSpriteCollision(int x1, int y1, int x2, int y2, int gap);
int isScrolledToScreen(int x, int y, int screenWidth);
int isScrolledOutOfScreen(int x, int y, int screenHeight);

void loadSprites(SDL_Renderer *renderer,
                 SDL_Texture **toasterTextures,
                 SDL_Texture **toastTexture);
void freeSprites(SDL_Texture **toasterTextures, SDL_Texture *toastTexture);

void setToasterSpawnCoordinates(struct Toaster *toaster, int screenWidth, int screenHeight);
void setToastSpawnCoordinates(struct Toast *toast, int screenWidth, int screenHeight);
void spawnToasters(struct Toaster *toasters, int screenWidth, int screenHeight, int *grid);
void spawnToasts(struct Toast *toasts, int screenWidth, int screenHeight, int *grid);

void drawSprite(SDL_Renderer *renderer, SDL_Texture *texture, int x, int y);

int *initGrid(void);

#endif
