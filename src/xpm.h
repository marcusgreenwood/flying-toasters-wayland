#ifndef XPM_H
#define XPM_H

#include <SDL.h>

/* Parse XPM data (array of strings) into an SDL_Surface with transparency.
 * Returns NULL on error. Caller must free with SDL_FreeSurface. */
SDL_Surface *xpm_to_surface(const char *const *xpm_data);

#endif
