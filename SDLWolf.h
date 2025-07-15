#pragma once
#include "Burger.h"
#include "res.h"
#include <SDL3/SDL.h>

extern SDL_Renderer *SdlRenderer;
extern RFILE *MainResources;
extern SDL_Keycode KeyBinds[12];

extern const char *PrefPath(void);
extern SDL_Storage *PrefStorage(void);
extern bool EnumerateLevels(SDL_EnumerateDirectoryCallback callback, void *userdata);
extern void SetPalette(SDL_Palette *Palette, unsigned char *PalPtr);
extern void BlitSurface(SDL_Surface *Surface, const SDL_Rect *rect);
extern void BlitScreen(void);
extern void StartUIOverlay(void);
extern void EndUIOverlay(void);
extern SDL_Surface *LoadPict(RFILE *Rp, Word PicNum);
