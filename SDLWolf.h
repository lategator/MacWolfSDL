#pragma once
#include "Burger.h"
#include "res.h"
#include <SDL3/SDL.h>

typedef uint32_t __attribute__((aligned(1), may_alias)) u_uint32_t;
typedef uint16_t __attribute__((aligned(1), may_alias)) u_uint16_t;

extern SDL_Window *SdlWindow;
extern SDL_Renderer *SdlRenderer;
extern SDL_Surface *CurrentSurface;
extern RFILE *MainResources;
extern SDL_Keycode KeyBinds[12];
extern char *ScenarioPath;

extern const char *PrefPath(void);
extern SDL_Storage *PrefStorage(void);
extern void EnumerateLevels(SDL_EnumerateDirectoryCallback callback, void *userdata);
extern void SetPalette(SDL_Palette *Palette, unsigned char *PalPtr);
extern void BlitSurface(SDL_Surface *Surface, const SDL_Rect *Rect);
extern void BlitScreen(void);
extern void StartUIOverlay(void);
extern void EndUIOverlay(void);
extern Boolean ProcessGlobalEvent(SDL_Event *Event);
extern SDL_Surface *LoadPict(RFILE *Rp, Word PicNum);
