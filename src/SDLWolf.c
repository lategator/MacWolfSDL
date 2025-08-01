#include "SDLWolf.h"
#include "Burger.h"
#include "WolfDef.h"
#include <stdlib.h>

#define TML_IMPLEMENTATION
#define TML_NO_STDIO
#define TML_MALLOC AllocSomeMem
#define TML_REALLOC SDL_realloc
#define TML_FREE FreeSomeMem
#define TML_MEMCPY SDL_memcpy
#include "tml.h"

#define TSF_NO_STDIO
#include "tsf.h"

#define INI_CUSTOM_ALLOCATOR 1
#define ini_malloc AllocSomeMem
#define ini_realloc SDL_realloc
#define ini_free FreeSomeMem
#include "ini.c"

static const char *PrefOrg = NULL;
static const char *PrefApp = "macwolfsdl";
static const char *PrefsFile = "macwolfsdl.ini";

#define NAUDIOCHANS 5
#define JOYDEADZONE 5000

Word MacWidth = 0;				/* Width of play screen (Same as GameRect.right) */
Word MacHeight = 0;				/* Height of play screen (Same as GameRect.bottom) */
Word MacViewHeight;				/* Height of 3d screen (Bottom of 3D view */
static const Word MonitorWidth=640;		/* Width of the monitor in pixels */
static const Word MonitorHeight=480;		/* Height of the monitor in pixels */
static const Word VidXs[] = {320,512,640,640};	/* Screen sizes to play with */
static const Word VidYs[] = {200,384,400,480};
static const Word VidVs[] = {160,320,320,400};
static const Word VidPics[] = {rFaceShapes,rFace512,rFace640,rFace640};		/* Resource #'s for art */
SDL_Window *SdlWindow = NULL;
SDL_Renderer *SdlRenderer = NULL;
SDL_Surface *CurrentSurface = NULL;
SDL_Texture *MacFontTexture = NULL;
static SDL_Surface *ScreenSurface = NULL;
static SDL_Texture *ScreenTexture = NULL;
static SDL_Texture *FramebufferTexture = NULL;
static SDL_Palette *ScreenPalette = NULL;
static SDL_AudioDeviceID SdlAudioDevice = 0;
static SDL_AudioStream *SdlSfxChannels[NAUDIOCHANS] = { NULL };
static SDL_AudioStream *SdlMusicStream;
static double SongMSecs;
static tml_message *SongMidiMessages = NULL;
static tml_message *SongCurrent;
static float *MusicBuffer = NULL;
static LongWord MusicBufferFrames = 0;
static Word SdlSfxNums[NAUDIOCHANS];
static Byte *GameShapeBuffer = NULL;
static char *MyPrefPath = NULL;
static SDL_Storage *MyPrefStorage = NULL;
static char *LastSaveDir = NULL;
static char *LastScenarioDir = NULL;
char *SaveFileName = NULL;

extern tsf *MusicSynth;
extern int SelectedMenu;
extern const unsigned char MacFont[];
extern const unsigned int MacFont_len;

static void CloseAudio(void);
static Boolean ChangeAudioDevice(SDL_AudioDeviceID ID, const SDL_AudioSpec *Fmt);
static void ProcessMusic(void *User, SDL_AudioStream *Stream, int Needed, int Total);
void MacLoadSoundFont(void);

void InitTools(void)
{
	SDL_JoystickID *Gamepads;
	int NumGamepads;
	int i;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
		BailOut("SDL_Init: %s", SDL_GetError());
	SDL_SetAppMetadata("Wolfenstein 3D", "1.0", "MacWolfSDL");
	Gamepads = SDL_GetGamepads(&NumGamepads);
	if (Gamepads) {
		for (i = 0; i < NumGamepads; i++)
			SDL_OpenJoystick(Gamepads[i]);
		SDL_free(Gamepads);
	}
	LoadPrefs();
	InitResources();
	MacLoadSoundFont();
	ChangeAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
	GetTableMemory();
	LoadMapSetData();
	NewGameWindow(2);				/* Create a game window at 512x384 */
	ClearTheScreen(BLACK);			/* Force the offscreen memory blank */
	BlastScreen();
}

const char *PrefPath(void)
{
	if (MyPrefPath)
		return MyPrefPath;
	MyPrefPath = SDL_GetPrefPath(PrefOrg, PrefApp);
	if (!MyPrefPath) BailOut("SDL_GetPrefPath: %s", SDL_GetError());
	return MyPrefPath;
}

SDL_Storage *PrefStorage(void)
{
	SDL_Storage *Storage;

	if (MyPrefStorage)
		return MyPrefStorage;
	Storage = SDL_OpenUserStorage(PrefOrg, PrefApp, 0);
	if (!Storage)
		return NULL;
	MyPrefStorage = Storage;
	while (!SDL_StorageReady(Storage)) {
		SDL_Delay(1);
	}
	return Storage;
}

Byte CurrentPal[768];

void SetPalette(SDL_Palette *Palette, unsigned char *PalPtr)
{
	Word i;					/* Temp */
	SDL_Color Colors[256];

	Colors[0] = (SDL_Color) { 255, 255, 255, 255 };
	i = 1;			/* Skip color #0 */
	PalPtr+=3;
	do {			/* Fill in all the color entries */
		Colors[i].r = PalPtr[0];
		Colors[i].g = PalPtr[1];
		Colors[i].b = PalPtr[2];
		Colors[i].a = 255;
		if (!Colors[i].b) {
			Colors[i].b = 0x01;
		}
		PalPtr+=3;
	} while (++i<256);	/* All done? */
	SDL_SetPaletteColors(Palette, Colors, 0, 256);
}

void SetAPalettePtr(unsigned char *PalPtr)
{
	memcpy(CurrentPal,PalPtr,768);
	SetPalette(ScreenPalette, PalPtr);
}

static void Cleanup(int status)
{
	SDL_JoystickID *Gamepads;
	SDL_Gamepad *Gamepad;
	int NumGamepads;
	int i;

	Gamepads = SDL_GetGamepads(&NumGamepads);
	if (Gamepads) {
		for (i = 0; i < NumGamepads; i++) {
			Gamepad = SDL_GetGamepadFromID(Gamepads[i]);
			if (Gamepad)
				SDL_CloseGamepad(Gamepad);
		}
		SDL_free(Gamepads);
	}
	CloseAudio();
	if (SongMidiMessages)
		tml_free(SongMidiMessages);
	if (MusicSynth)
		tsf_close(MusicSynth);
	if (ScreenSurface)
		SDL_DestroySurface(ScreenSurface);
	if (ScreenTexture)
		SDL_DestroyTexture(ScreenTexture);
	if (FramebufferTexture)
		SDL_DestroyTexture(FramebufferTexture);
	if (SdlRenderer)
		SDL_DestroyRenderer(SdlRenderer);
	if (SdlWindow)
		SDL_DestroyWindow(SdlWindow);
	if (MyPrefPath)
		SDL_free(MyPrefPath);
	KillResources();
	if (MyPrefStorage)
		SDL_CloseStorage(MyPrefStorage);

	SDL_Quit();
	SDL_free(SaveFileName);
	SDL_free(LastSaveDir);
	SDL_free(LastScenarioDir);
	exit(status);
}

void GoodBye(void)
{
	Cleanup(0);
}

static void BailOut2(const char *Msg)
{
	fprintf(stderr, "%s\n", Msg);
	UngrabMouse();
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", Msg, SdlWindow);
	Cleanup(1);
}

void BailOut(const char *Fmt, ...)
{
	char Buf[4096];
	va_list Args;

	va_start(Args, Fmt);
	vsnprintf(Buf, sizeof Buf, Fmt, Args);
	va_end(Args);
	BailOut2(Buf);
}

static void ClearScreenTexture(void)
{
	SDL_Surface *Screen;
	SDL_LockTextureToSurface(ScreenTexture, NULL, &Screen);
	SDL_ClearSurface(Screen, 0, 0, 0, 0);
	SDL_UnlockTexture(ScreenTexture);
}

void BlitSurface(SDL_Surface *Surface, const SDL_Rect *Rect)
{
	SDL_Surface *Screen;
	Boolean Lock = (Surface->flags & SDL_SURFACE_LOCKED) != 0;
	SDL_LockTextureToSurface(ScreenTexture, NULL, &Screen);
	if (Lock)
		SDL_UnlockSurface(Surface);
	SDL_BlitSurface(Surface, NULL, Screen, Rect);
	if (Lock)
		SDL_LockSurface(Surface);
	SDL_UnlockTexture(ScreenTexture);
}

void BlitScenarioSelect(SDL_Surface *BG, SDL_Surface *Pic, const SDL_Rect* Rect)
{
	SDL_Surface* Screen;

	SDL_LockTextureToSurface(ScreenTexture, NULL, &Screen);
	SDL_ClearSurface(Screen, 0, 1, 1, 1);
	if (BG)
		SDL_BlitSurface(BG, NULL, Screen, NULL);
	if (Pic)
		SDL_BlitSurface(Pic, NULL, Screen, Rect);
	SDL_UnlockTexture(ScreenTexture);
}

void BlitScreen(void)
{
	BlitSurface(ScreenSurface, NULL);
}

static void SetPresentationMode(void)
{
	SDL_RendererLogicalPresentation Present = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
	switch (ScreenScaleMode) {
		case 0: Present = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE; break;
		case 1: Present = SDL_LOGICAL_PRESENTATION_LETTERBOX; break;
		case 2: Present = SDL_LOGICAL_PRESENTATION_STRETCH; break;
		default: break;
	}
	SDL_SetRenderLogicalPresentation(SdlRenderer, MacWidth, MacHeight, Present);
}

void ClearFrameBuffer(void)
{
	SDL_SetRenderTarget(SdlRenderer, FramebufferTexture);
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 0);
	SDL_RenderClear(SdlRenderer);
}

void RenderScreen(void)
{
	SDL_SetRenderTarget(SdlRenderer, FramebufferTexture);
	SDL_RenderTexture(SdlRenderer, ScreenTexture, NULL, NULL);
}

void PresentScreen(void)
{
	SDL_SetRenderTarget(SdlRenderer, NULL);
	SetPresentationMode();
	SDL_RenderTexture(SdlRenderer, FramebufferTexture, NULL, NULL);
	SDL_RenderPresent(SdlRenderer);
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 0);
	SDL_RenderClear(SdlRenderer);
}

void BlastScreen(void)
{
	BlitScreen();
	RenderScreen();
	PresentScreen();
}

void BlastScreen2(Rect *BlastRect)
{
	BlastScreen();
}

Boolean ProcessGlobalEvent(SDL_Event *Event)
{
	SDL_Gamepad *Gamepad;
	switch (Event->type) {
		case SDL_EVENT_QUIT:
			GoodBye();
			__builtin_unreachable();
		case SDL_EVENT_WINDOW_RESIZED:
			PresentScreen();
			return TRUE;
		case SDL_EVENT_GAMEPAD_ADDED:
			SDL_OpenGamepad(Event->gdevice.which);
			return TRUE;
		case SDL_EVENT_GAMEPAD_REMOVED:
			Gamepad = SDL_GetGamepadFromID(Event->gdevice.which);
			if (Gamepad)
				SDL_CloseGamepad(Gamepad);
			return TRUE;
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			if (Event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX)
				joystickx = Event->gaxis.value;
			else if (Event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)
				joysticky = Event->gaxis.value;
			return TRUE;
			break;
	}
	return FALSE;
}

/**********************************

	Read from the keyboard/mouse system

**********************************/

SDL_Keycode KeyBinds[12] = {
	SDL_SCANCODE_X,
	SDL_SCANCODE_Z,
	SDL_SCANCODE_LALT,
	SDL_SCANCODE_SPACE,
	SDL_SCANCODE_RIGHT,
	SDL_SCANCODE_LEFT,
	SDL_SCANCODE_DOWN,
	SDL_SCANCODE_UP,
	SDL_SCANCODE_SLASH,
	SDL_SCANCODE_TAB,
	SDL_SCANCODE_LSHIFT,
	SDL_SCANCODE_LCTRL,
};

typedef struct {
	SDL_Scancode Code;
	Word JoyValue;
} Keys2Joy;

typedef struct {
	SDL_GamepadButton Code;
	Word JoyValue;
} Pad2Joy;

static const Pad2Joy GamepadMatrix[] = {
	{SDL_GAMEPAD_BUTTON_DPAD_UP,JOYPAD_UP},
	{SDL_GAMEPAD_BUTTON_DPAD_DOWN,JOYPAD_DN},
	{SDL_GAMEPAD_BUTTON_DPAD_LEFT,JOYPAD_LFT},
	{SDL_GAMEPAD_BUTTON_DPAD_RIGHT,JOYPAD_RGT},
	{ SDL_GAMEPAD_BUTTON_SOUTH,JOYPAD_B},
	{ SDL_GAMEPAD_BUTTON_EAST,JOYPAD_A},
	{ SDL_GAMEPAD_BUTTON_NORTH,JOYPAD_X},
	{ SDL_GAMEPAD_BUTTON_WEST,JOYPAD_Y},
	{ SDL_GAMEPAD_BUTTON_BACK,JOYPAD_SELECT},
	{ SDL_GAMEPAD_BUTTON_START,JOYPAD_START},
	{ SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,JOYPAD_TR},
	{ SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,JOYPAD_TL},
};

static const Keys2Joy MenuKeyMatrix[] = {
	{SDL_SCANCODE_UP,JOYPAD_UP},
	{SDL_SCANCODE_DOWN,JOYPAD_DN},
	{SDL_SCANCODE_LEFT,JOYPAD_LFT},
	{SDL_SCANCODE_RIGHT,JOYPAD_RGT},
	{ SDL_SCANCODE_SPACE,JOYPAD_A},
	{ SDL_SCANCODE_RETURN,JOYPAD_A},
	{ SDL_SCANCODE_KP_ENTER,JOYPAD_A},
	{ SDL_SCANCODE_ESCAPE,JOYPAD_B},
};

static const Pad2Joy MenuPadMatrix[] = {
	{SDL_GAMEPAD_BUTTON_DPAD_UP,JOYPAD_UP},
	{SDL_GAMEPAD_BUTTON_DPAD_DOWN,JOYPAD_DN},
	{SDL_GAMEPAD_BUTTON_DPAD_LEFT,JOYPAD_LFT},
	{SDL_GAMEPAD_BUTTON_DPAD_RIGHT,JOYPAD_RGT},
	{ SDL_GAMEPAD_BUTTON_SOUTH,JOYPAD_A},
	{ SDL_GAMEPAD_BUTTON_EAST,JOYPAD_B},
	{ SDL_GAMEPAD_BUTTON_BACK,JOYPAD_B},
};

static const char *CheatPtr[] = {		/* Cheat strings */
	"XUSCNIELPPA",
	"IDDQD",
	"BURGER",
	"WOWZERS",
	"LEDOUX",
	"SEGER",
	"MCCALL",
	"APPLEIIGS"
};
static Word Cheat;			/* Which cheat is active */
static Word CheatIndex;	/* Index to the cheat string */

exit_t ReadSystemJoystick(void)
{
	Word i;
	Word Index;
	SDL_Event event;
	const bool *Keys;
	const Pad2Joy *PadPtr;
	const SDL_Keycode *KeyPtr = KeyBinds;

	joystick1 = 0;			/* Assume that joystick not moved */
	mousewheel = 0;

	/* Switch weapons like in DOOM! */
	PauseExited = FALSE;
	SDL_PumpEvents();
	while (SDL_PollEvent(&event)) {
		if (ProcessGlobalEvent(&event))
			continue;
		if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			mousewheel += event.wheel.y;
		} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			mousebuttons |= SDL_BUTTON_MASK(event.button.button);
		} else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
			mousebuttons &= ~SDL_BUTTON_MASK(event.button.button);
		} else if ((event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_ESCAPE)
			|| event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
			PauseExited = TRUE;
			return PauseMenu(TRUE);
		} else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
			i = SDL_toupper(event.key.key);	/* Force UPPER case */
			if (CheatIndex) {		/* Cheat in progress */
				if (CheatPtr[Cheat][CheatIndex]==i) {		/* Match the current string? */
					++CheatIndex;				/* Next char */
					if (!CheatPtr[Cheat][CheatIndex]) {	/* End of the string? */
						PlaySound(SND_BONUS);		/* I got a bonus! */
						switch (Cheat) {		/* Execute the cheat */
							case 1:
								gamestate.godmode^=TRUE;			/* I am invincible! */
								break;
							case 5:
								GiveKey(0);
								GiveKey(1);		/* Award the keys */
								break;
							case 6:
								playstate=EX_WARPED;		/* Force a jump to the next level */
								nextmap = gamestate.mapon+1;	/* Next level */
								if (MapListPtr->MaxMap<=nextmap) {	/* Too high? */
									nextmap = 0;			/* Reset to zero */
								}
								break;
							case 7:
								ShowPush ^= TRUE;
								break;
							case 0:
							case 4:
								GiveKey(0);		/* Award the keys */
								GiveKey(1);
								gamestate.godmode = TRUE;			/* I am a god */
							case 2:
								gamestate.machinegun = TRUE;
								gamestate.chaingun = TRUE;
								gamestate.flamethrower = TRUE;
								gamestate.missile = TRUE;
								GiveAmmo(gamestate.maxammo);
								GiveGas(99);
								GiveMissile(99);
								break;
							case 3:
								gamestate.maxammo = 999;
								GiveAmmo(999);
						}
					}
				} else {
					CheatIndex = 0;
					goto TryFirst;
				}
			} else {
			TryFirst:
				Index = 0;				/* Init the scan routine */
				do {
					if (CheatPtr[Index][0] == i) {
						Cheat = Index;		/* This is my current cheat I am scanning */
						CheatIndex = 1;		/* Index to the second char */
						break;				/* Exit */
					}
				} while (++Index<8);		/* All words scanned? */
			}
			switch (i) {		/* Use the SCAN code to make sure I hit the right key! */
				case SDLK_1 :		/* 1 */
					gamestate.pendingweapon = WP_KNIFE;
					break;
				case SDLK_2 : 	/* 2 */
					if (gamestate.ammo) {
						gamestate.pendingweapon = WP_PISTOL;
					}
					break;
				case SDLK_3 :		/* 3 */
					if (gamestate.ammo && gamestate.machinegun) {
						gamestate.pendingweapon = WP_MACHINEGUN;
					}
					break;
				case SDLK_4 :		/* 4 */
					if (gamestate.ammo && gamestate.chaingun) {
						gamestate.pendingweapon = WP_CHAINGUN;
					}
					break;
				case SDLK_5 :		/* 5 */
					if (gamestate.gas && gamestate.flamethrower) {
						gamestate.pendingweapon = WP_FLAMETHROWER;
					}
					break;
				case SDLK_6 :		/* 6 */
					if (gamestate.missiles && gamestate.missile) {
						gamestate.pendingweapon = WP_MISSILE;
					}
					break;
			}
			if (event.key.scancode == KeyBinds[8])
				joystick1 = JOYPAD_START;
		} else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
			PadPtr = GamepadMatrix;
			for (i = 0; i < ARRAYLEN(GamepadMatrix); i++) {
				if (PadPtr->Code == event.gbutton.button) {
					if (!event.key.repeat || (PadPtr->JoyValue != JOYPAD_A && PadPtr->JoyValue != JOYPAD_B))
						joystick1 |= PadPtr->JoyValue;
					break;
				}
				PadPtr++;					/* Next index */
			}
		} else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
			if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
				if (joystick1&JOYPAD_TR) {	/* Strafing? */
					mousex += (event.gaxis.value - joystickx)/0x100;
				} else {
					mouseturn -= (event.gaxis.value - joystickx)/0x100;
				}
				joystickx = event.gaxis.value;
			} else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
				mousey += (event.gaxis.value - joysticky)/0x100;
				joysticky = event.gaxis.value;
			}
		} else if (event.type == SDL_EVENT_MOUSE_MOTION && MouseEnabled) {
			if (joystick1&JOYPAD_TR) {	/* Strafing? */
				mousex += event.motion.xrel;	/* Move horizontally for strafe */
			} else {
				mouseturn -= event.motion.xrel;	/* Turn left or right */
			}
			mousey += event.motion.yrel;		/* Forward motion */
		}
	}
	if (joystick1 & JOYPAD_START)
		return 0;
	Keys = SDL_GetKeyboardState(NULL);
	i = 0;					/* Init the count */
	do {
		if (i != 8 && Keys[*KeyPtr])
			joystick1 |= 1<<(i+4);	/* Set the joystick value */
		KeyPtr++;					/* Next index */
	} while (++i<ARRAYLEN(KeyBinds));				/* All done? */
	if (MouseEnabled) {
		if (mousebuttons & SDL_BUTTON_LMASK)
			joystick1 |= JOYPAD_B;
		//if (mousebuttons & SDL_BUTTON_RMASK)
			//joystick1 |= JOYPAD_A;
	}

	if (joystick1 & JOYPAD_X) {		/* Handle the side scroll (Special case) */
		if (joystick1&JOYPAD_LFT) {
			joystick1 = (joystick1 & ~JOYPAD_LFT) | JOYPAD_TL;
		} else if (joystick1&JOYPAD_RGT) {
			joystick1 = (joystick1 & ~JOYPAD_RGT) | JOYPAD_TR;
		}
		joystick1 &= ~JOYPAD_X;
	}
	return 0;
}

int ReadMenuJoystick(void)
{
	Word i;
	SDL_Event event;
	const Keys2Joy *KeyPtr;
	const Pad2Joy *PadPtr;
	int oldjoyx, oldjoyy;

	joystick1 = 0;
	mousewheel = 0;

	SDL_PumpEvents();
	while (SDL_PollEvent(&event)) {
		oldjoyx = joystickx;
		oldjoyy = joysticky;
		if (ProcessGlobalEvent(&event)) {
			if (joystickx >= JOYDEADZONE && oldjoyx < JOYDEADZONE)
				joystick1 |= JOYPAD_RGT;
			else if (joystickx <= -JOYDEADZONE && oldjoyx > JOYDEADZONE)
				joystick1 |= JOYPAD_LFT;
			if (joysticky >= JOYDEADZONE && oldjoyy < JOYDEADZONE)
				joystick1 |= JOYPAD_DN;
			else if (joysticky <= -JOYDEADZONE && oldjoyy > JOYDEADZONE)
				joystick1 |= JOYPAD_UP;
			if (event.type == SDL_EVENT_WINDOW_RESIZED)
				return SDL_MAX_SINT32;
			continue;
		}
		SDL_ConvertEventToRenderCoordinates(SdlRenderer, &event);
		if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			mousewheel += event.wheel.y;
		} else if (event.type == SDL_EVENT_MOUSE_MOTION) {
			mousex = event.motion.x;
			mousey = event.motion.y;
		} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			mousex = event.motion.x;
			mousey = event.motion.y;
			return event.button.button;
		} else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
			PadPtr = MenuPadMatrix;
			for (i = 0; i < ARRAYLEN(MenuPadMatrix); i++) {
				if (PadPtr->Code == event.gbutton.button) {
					if (!event.key.repeat || (PadPtr->JoyValue != JOYPAD_A && PadPtr->JoyValue != JOYPAD_B))
						joystick1 |= PadPtr->JoyValue;
					break;
				}
				PadPtr++;					/* Next index */
			}
		} else if (event.type == SDL_EVENT_KEY_DOWN) {
			KeyPtr = MenuKeyMatrix;
			for (i = 0; i < ARRAYLEN(MenuKeyMatrix); i++) {
				if (KeyPtr->Code == event.key.scancode) {
					if (!event.key.repeat || (KeyPtr->JoyValue != JOYPAD_A && KeyPtr->JoyValue != JOYPAD_B))
						joystick1 |= KeyPtr->JoyValue;
					break;
				}
				KeyPtr++;					/* Next index */
			}
		}
	}
	return 0;
}

void UpdateVideoSettings(void)
{
	SDL_SetTextureScaleMode(FramebufferTexture, ScreenFilter);
}

static void InitMacFont(void)
{
	const uint8_t *FontStart;
	SDL_Surface *FontSurface;
	SDL_Surface *FontTexSurface;
	SDL_Palette *Palette;
	int FontHeight;
	static const SDL_Color Font1BitColors[2] = {{ 0, 0, 0, 0}, {255, 255, 255, 255}};

	if (MacFontTexture) {
		SDL_DestroyTexture(MacFontTexture);
		MacFontTexture = NULL;
	}

	FontStart = (95+95)*4+MacFont;
	FontHeight = (MacFont+MacFont_len-FontStart)/(128/8);
	FontSurface = SDL_CreateSurfaceFrom(128, FontHeight, SDL_PIXELFORMAT_INDEX1LSB, (void*)FontStart, 128/8);
	if (FontSurface) {
		Palette = SDL_CreateSurfacePalette(FontSurface);
		if (Palette) {
			SDL_SetPaletteColors(Palette, Font1BitColors, 0, ARRAYLEN(Font1BitColors));
			FontTexSurface = SDL_ConvertSurface(FontSurface, SDL_PIXELFORMAT_ABGR8888);
			SDL_DestroySurface(FontSurface);
			if (FontTexSurface) {
				MacFontTexture = SDL_CreateTextureFromSurface(SdlRenderer, FontTexSurface);
				SDL_DestroySurface(FontTexSurface);
			}
		}
	}
}

void ResizeGameWindow(Word Width, Word Height)
{
	if (Width == MacWidth && Height == MacHeight)
		return;
	if ((Width != MacWidth || Height != MacHeight) && MathSize != (Word)-1)
		MathSize = -2;
	MacWidth = Width;
	MacHeight = Height;
	MacViewHeight = Height;
	MacVidSize = -1;
	if (!SdlWindow) {
		SdlWindow = SDL_CreateWindow("Wolfenstein 3D", MacWidth, MacHeight, SDL_WINDOW_RESIZABLE);
		if (!SdlWindow)
			BailOut("SDL_CreateWindow: %s", SDL_GetError());
		SdlRenderer = SDL_CreateRenderer(SdlWindow, NULL);
		if (!SdlRenderer)
			BailOut("SDL_CreateRenderer: %s", SDL_GetError());
		InitMacFont();
	} else {
		if (ScreenSurface)
			SDL_DestroySurface(ScreenSurface);
		if (ScreenTexture)
			SDL_DestroyTexture(ScreenTexture);
		if (FramebufferTexture)
			SDL_DestroyTexture(FramebufferTexture);
		ScreenSurface = NULL;
		ScreenTexture = NULL;
		FramebufferTexture = NULL;
	}
	SDL_SetWindowMinimumSize(SdlWindow, MacWidth, MacHeight);
	SDL_SetWindowFullscreen(SdlWindow, FullScreen);
	SDL_SetRenderTarget(SdlRenderer, NULL);
	SetPresentationMode();
	ScreenSurface = SDL_CreateSurface(MacWidth, MacHeight, SDL_PIXELFORMAT_INDEX8);
	if (!ScreenSurface)
		BailOut("SDL_CreateSurface: %s", SDL_GetError());
	CurrentSurface = ScreenSurface;
	ScreenPalette = SDL_CreateSurfacePalette(ScreenSurface);
	if (!ScreenPalette)
		BailOut("SDL_CreateSurfacePalette: %s", SDL_GetError());
	ScreenTexture = SDL_CreateTexture(SdlRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, MacWidth, MacHeight);
	if (!ScreenTexture)
		BailOut("SDL_CreateTexture: %s", SDL_GetError());
	FramebufferTexture = SDL_CreateTexture(SdlRenderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, MacWidth, MacHeight);
	if (!FramebufferTexture)
		BailOut("SDL_CreateTexture: %s", SDL_GetError());

	SDL_SetTextureBlendMode(ScreenTexture, SDL_BLENDMODE_NONE);
	SDL_SetTextureBlendMode(FramebufferTexture, SDL_BLENDMODE_NONE);

	if (SDL_MUSTLOCK(ScreenSurface))
		SDL_LockSurface(ScreenSurface);
	VideoPointer = ScreenSurface->pixels;
	VideoWidth = ScreenSurface->pitch;
	InitYTable();				/* Init the game's YTable */
	UpdateVideoSettings();
}

Word NewGameWindow(Word NewVidSize)
{
	LongWord *LongPtr;
	LongWord UnpackLength;
	LongWord Offset;
	Word i,j;
	Boolean Pass2;

	Pass2 = FALSE;		/* Assume memory is OK */

	if (NewVidSize>=2) {	/* Is this 640 mode? */
		if (MonitorWidth<640) {	/* Can I show 640 mode? */
			NewVidSize=1;	/* Reduce to 512 mode */
		} else {
			if (MonitorHeight<480) {	/* Can I display 480 lines? */
				NewVidSize=2;	/* Show 400 instead */
			}
		}
	}
	if (NewVidSize==MacVidSize) {	/* Same size being displayed? */
		return MacVidSize;				/* Exit then... */
	}
	SelectedMenu = -1;
TryAgain:
	if (GameShapeBuffer) {
		FreeSomeMem(GameShapeBuffer);	/* All the permanent game shapes */
		GameShapeBuffer=0;
		for (i = 0; i < 57; i++)
			GameShapes[i] = NULL;
	}

	ResizeGameWindow(VidXs[NewVidSize], VidYs[NewVidSize]);
	MacVidSize = NewVidSize;	/* Set the new data size */
	MacViewHeight = VidVs[NewVidSize];

	SetAPalette(rBlackPal);		/* Set the video palette */
	ClearTheScreen(BLACK);		/* Set the screen to black */
	BlastScreen();

	GameShapeBuffer = LoadCompressed(VidPics[MacVidSize], &UnpackLength);	/* All the permanent game shapes */
	if (!GameShapeBuffer)
		goto OhShit;
	i = 0;
	j = (MacVidSize==1) ? 47+10 : 47;		/* 512 mode has 10 shapes more */
	LongPtr = (LongWord *) GameShapeBuffer;
	if (UnpackLength < j * sizeof(LongWord))
		goto OhShit;
	do {
		Offset = SwapLongBE(LongPtr[i]);
		if (UnpackLength < Offset + 4)
			goto OhShit;
		Word *ShapePtr = (void*)(GameShapeBuffer+Offset);
		if (i >= 12 && i < 12+24) {
			if (UnpackLength < Offset + 8)
				goto OhShit;
			if (UnpackLength < Offset + 4 + SwapUShortBE(ShapePtr[2]) * SwapUShortBE(ShapePtr[3]))
				goto OhShit;
		} else {
			if (UnpackLength < Offset + 4 + SwapUShortBE(ShapePtr[0]) * SwapUShortBE(ShapePtr[1]))
				goto OhShit;
		}
		GameShapes[i] = ShapePtr;
	} while (++i<j);
	if (Pass2) {		/* Low memory? */
		if (!StartupRendering(NewVidSize)) {		/* Reset the scalers... */
			ReleaseScalers();
			goto OhShit;
		}
	}
	return MacVidSize;

OhShit:			/* Oh oh.... */
	if (Pass2) {
		if (!NewVidSize) {		/* At the smallest screen size? */
			BailOut("Game data is corrupted");
		}
		--NewVidSize;			/* Smaller size */
	} else {
		PlaySong(0);			/* Release song memory */
		ReleaseScalers();		/* Release the compiled scalers */
		//PurgeAllSounds(1000000);	/* Force sounds to be purged */
		Pass2 = TRUE;
	}
	goto TryAgain;				/* Let's try again */
}

void GrabMouse(void)
{
	if (MouseEnabled)
		SDL_SetWindowRelativeMouseMode(SdlWindow, TRUE);
}

void UngrabMouse(void)
{
	SDL_SetWindowRelativeMouseMode(SdlWindow, FALSE);
}

/**********************************

	Scale the system X coord

**********************************/

Word ScaleX(Word x)
{
	switch(MacVidSize) {
	case 1:
		return x*8/5;
	case 2:
	case 3:
		return x*2;
	}
	return x;
}

/**********************************

	Scale the system Y coord

**********************************/

Word ScaleY(Word y)
{
	switch(MacVidSize) {
	case 1:				/* 512 resolution */
		y = (y*8/5)+64;
		if (y==217) {	/* This hack will line up the gun on 512 mode */
			++y;
		}
		return y;
	case 2:				/* 640 x 400 */
		return y*2;
	case 3:				/* 640 x 480 */
		return y*2+80;
	}
	return y;			/* 320 resolution */
}

/**********************************

	Handle GET PSYCHED!

**********************************/

static SDL_FRect PsychedRect;
#define PSYCHEDWIDE 184
#define PSYCHEDHIGH 5
#define PSYCHEDX 20
#define PSYCHEDY 46
#define MAXINDEX (66+S_LASTONE)

/**********************************

	Draw the initial shape

**********************************/

void ShowGetPsyched(void)
{
	Byte *ShapePtr;
	Word X,Y;

	X = (MacWidth-224)/2;
	Y = (MacViewHeight-56)/2;
	ClearTheScreen(BLACK);
	BlastScreen();
	ShapePtr = LoadCompressedShape(rGetPsychPic);
	if (ShapePtr) {
		DrawShape(X,Y,ShapePtr);
		FreeSomeMem(ShapePtr);
		BlastScreen();
	}
	SetAPalette(rGamePal);
	PsychedRect.y = Y + PSYCHEDY;
	PsychedRect.h = PSYCHEDHIGH;
	PsychedRect.x = X + PSYCHEDX;
	PsychedRect.w = 0;
}

void DrawPsyched(Word Index)
{
	float Factor;

	Factor = Index * PSYCHEDWIDE;		/* Calc the relative X pixel factor */
	Factor = Factor / (float) MAXINDEX;
	PsychedRect.w = SDL_floorf(Factor);
	BlitScreen();
	SDL_SetRenderTarget(SdlRenderer, FramebufferTexture);
	SDL_RenderTexture(SdlRenderer, ScreenTexture, NULL, NULL);
	SDL_SetRenderDrawColor(SdlRenderer, 255, 0, 0, 255);
	SDL_RenderFillRect(SdlRenderer, &PsychedRect);
	PresentScreen();
}

void EndGetPsyched(void)
{
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
	SetAPalette(rBlackPal);		/* Zap the palette */
}

void ShareWareEnd(void)
{
}


/**********************************

	Save the game

**********************************/

Byte MachType[4] = "SDL3";

#define WRITECHECKED(src, size) do { \
		if (fwrite((src), 1, (size), FileRef) != (size)) \
			goto Bogus; \
	} while (0)

void SaveGame(void)
{
	char Err[1024];
	uint32_t StrLen;
	Word PWallWord;
	FILE *FileRef;

	if (!SaveFileName)
		return;
	if (playstate != EX_STILLPLAYING && playstate != EX_AUTOMAP)
		return;
	FileRef = fopen(SaveFileName, "wb");
	if (!FileRef) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to save game", SdlWindow);
		return;
	}

	WRITECHECKED(&MachType, 4);							/* Save a machine type ID */
	StrLen = strlen(ScenarioPath);
	WRITECHECKED(&StrLen, 4);	/* Save scenario path len */
	WRITECHECKED(ScenarioPath, StrLen);	/* Save full path to scenario file */
	WRITECHECKED(&gamestate, sizeof(gamestate));		/* Save the game stats */
	WRITECHECKED(&PushWallRec, sizeof(PushWallRec));	/* Save the pushwall stats */

	WRITECHECKED(&nummissiles, sizeof(nummissiles));	/* Save missiles in motion */
	if (nummissiles)
		WRITECHECKED(&missiles[0], nummissiles*sizeof(missile_t));

	WRITECHECKED(&numactors, sizeof(numactors));		/* Save actors */
	if (numactors)
		WRITECHECKED(&actors[0], numactors*sizeof(actor_t));

	WRITECHECKED(&numdoors, sizeof(numdoors));		/* Save doors */
	if (numdoors)
		WRITECHECKED(&doors[0], numdoors*sizeof(door_t));

	WRITECHECKED(&numstatics, sizeof(numstatics));
	if (numstatics)
		WRITECHECKED(&statics[0], numstatics*sizeof(static_t));

	WRITECHECKED(MapPtr, 64*64);
	WRITECHECKED(&tilemap, sizeof(tilemap));				/* Tile map */

	WRITECHECKED(&ConnectCount, sizeof(ConnectCount));
	if (ConnectCount)
		WRITECHECKED(&areaconnect[0], ConnectCount * sizeof(connect_t));
	WRITECHECKED(&areabyplayer[0], sizeof(areabyplayer));
	WRITECHECKED(&textures[0], (128+5)*64);
	PWallWord = 0;		/* Assume no pushwall pointer in progress */
	if (pwallseg) {
		PWallWord = (pwallseg-(saveseg_t*)nodes)+1;		/* Convert to number offset */
	}
	WRITECHECKED(&PWallWord, sizeof PWallWord);
	WRITECHECKED(nodes, MapPtr->numnodes*sizeof(savenode_t));	/* Save BSP tree */
	fclose(FileRef);						/* Close the file */
	PlaySound(SND_BONUS);
	return;
Bogus:
	fclose(FileRef);						/* Close the file */
	SDL_RemovePath(SaveFileName);
	snprintf(Err, sizeof Err, "Failed to write save file: %s", SaveFileName);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", Err, SdlWindow);
}

/**********************************

	Load the game

**********************************/
void *SaveRecord;
void *SaveRecordMem;
LongWord SaveRecordSize;

#define CHECKSPACE(n) do { \
	if (FileSize < FilePtr.B - TheMem + (n)) \
		goto Bogus; \
	} while (0)

Boolean LoadGame(void)
{
	char Err[1024];
	LongWord FileSize;
	LongWord PathLen;
	char *Path = NULL;
	union {
		Byte *B;
		u_uint16_t *S;
		u_uint16_t *W;
		u_uint32_t *L;
	} FilePtr;
	Byte *TheMem;
	FILE *FileRef;

	if (!SaveFileName)
		return FALSE;
	FileRef = fopen(SaveFileName, "rb");
	if (!FileRef)
		return FALSE;
	if (fseek(FileRef, 0, SEEK_END))
		goto BadFile;
	FileSize = ftell(FileRef);
	if ((int)FileSize < 0)
		goto BadFile;
	if (fseek(FileRef, 0, SEEK_SET))
		goto BadFile;
	FilePtr.B = TheMem = AllocSomeMem(FileSize);	/* Get memory for the file */
	if (FileSize <= 8)
		goto Bogus;
	if (fread(FilePtr.B, 1, FileSize, FileRef) != FileSize)	/* Open the file */
		goto Bogus;
	fclose(FileRef);						/* Close the file */
	if (memcmp(MachType,FilePtr.B,4))		/* Is this the proper machine type? */
		goto Bogus;
	FilePtr.B+=4;							/* Index past signature */
	PathLen = *FilePtr.L;
	FilePtr.B+=4;							/* Index past length */
	CHECKSPACE(PathLen);
	Path = AllocSomeMem(PathLen+1);
	memcpy(Path, FilePtr.B, PathLen);
	Path[PathLen] = '\0';
	FilePtr.B+=PathLen;
	CHECKSPACE(sizeof(gamestate));
	MountMapFile(Path);
	LoadMapSetData();
	memcpy(&gamestate,FilePtr.B,sizeof(gamestate));	/* Reset the game state */
	SaveRecord = FilePtr.B;
	SaveRecordMem = TheMem;
	SaveRecordSize = FileSize;
	if (ScenarioPath)
		FreeSomeMem(ScenarioPath);
	ScenarioPath = Path;
	return TRUE;

Bogus:
	if (Path)
		FreeSomeMem(Path);
	FreeSomeMem(TheMem);
	snprintf(Err, sizeof Err, "Save file is corrupted: %s", SaveFileName);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", Err, SdlWindow);
	return FALSE;
BadFile:
	fclose(FileRef);
	snprintf(Err, sizeof Err, "Failed to read save file: %s", SaveFileName);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", Err, SdlWindow);
	return FALSE;
}

#define COPYCHECKED(dest, size) do { \
		CHECKSPACE((size)); \
		memcpy((dest),FilePtr.B,(size)); \
		FilePtr.B+=(size); \
	} while (0)

void FinishLoadGame(void)
{
	union {
		Byte *B;
		unsigned short *S;
		Word *W;
		LongWord *L;
	} FilePtr;
	LongWord FileSize;
	Byte *TheMem;
	Word Tmp;

	FilePtr.B = SaveRecord;
	TheMem = SaveRecordMem;
	FileSize = SaveRecordSize;

	COPYCHECKED(&gamestate,sizeof(gamestate));	/* Reset the game state */
	COPYCHECKED(&PushWallRec,sizeof(PushWallRec));				/* Record for the single pushwall in progress */

	COPYCHECKED(&nummissiles,sizeof nummissiles);
	if (nummissiles)
		COPYCHECKED(missiles,sizeof(missile_t)*nummissiles);

	COPYCHECKED(&numactors,sizeof numactors);
	if (numactors)
		COPYCHECKED(actors,sizeof(actor_t)*numactors);

	COPYCHECKED(&numdoors,sizeof numdoors);
	if (numdoors)
		COPYCHECKED(doors,sizeof(door_t)*numdoors);

	COPYCHECKED(&numstatics,sizeof numstatics);
	if (numstatics)
		COPYCHECKED(statics,sizeof(static_t)*numstatics);

	COPYCHECKED(MapPtr,MAPSIZE*MAPSIZE);
	COPYCHECKED(tilemap,sizeof tilemap);						/* Tile map */

	COPYCHECKED(&ConnectCount,sizeof ConnectCount);		/* Number of valid interconnects */
	if (ConnectCount)
		COPYCHECKED(areaconnect,sizeof(connect_t)*ConnectCount);	/* Is this area mated with another? */
	COPYCHECKED(areabyplayer,sizeof(areabyplayer));	/* Which areas can I see into? */

	COPYCHECKED(textures,sizeof textures);				/* Texture array for pushwalls */

	pwallseg = 0;			/* Assume bogus */
	COPYCHECKED(&Tmp,sizeof Tmp);				/* Texture array for pushwalls */
	if (Tmp) {
		pwallseg = (saveseg_t *)nodes;
		pwallseg = &pwallseg[Tmp-1];
	}
	COPYCHECKED(nodes,MapPtr->numnodes*sizeof(savenode_t));

	FreeSomeMem(SaveRecordMem);
	return;
Bogus:
	BailOut("Save file is corrupted: %s", SaveFileName);
}

#undef COPYCHECKED
#undef CHECKSPACE

/**********************************

	Show the standard file dialog
	for opening a new game

**********************************/

static SDL_AtomicInt DialogStatus;

#ifdef SDL_PLATFORM_WINDOWS
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

static inline void SetupFileDialog(void)
{
	SDL_SetAtomicInt(&DialogStatus, -1);
}

static Boolean WaitDialog(void)
{
	SDL_Event Event;
	int Status;

	for (;;) {
		SDL_WaitEvent(&Event);
		ProcessGlobalEvent(&Event);
		Status = SDL_GetAtomicInt(&DialogStatus);
		if (Status >= 0)
			break;
	}
	return Status;
}

static void ScenarioFileChosen(void *userdata, const char * const *filelist, int filter)
{
	SDL_Event Event;
	char *Dir;

	if (!filelist || !*filelist) {
		SDL_SetAtomicInt(&DialogStatus, 0);
	} else {
		SDL_SetAtomicInt(&DialogStatus, 1);
		if (NextScenarioPath)
			SDL_free(NextScenarioPath);
		NextScenarioPath = SDL_strdup(*filelist);
		if (!NextScenarioPath) BailOut("Out of memory");
		if (LastScenarioDir) {
			SDL_free(LastScenarioDir);
			LastScenarioDir = NULL;
		}
		Dir = SDL_strrchr(NextScenarioPath, PATH_SEP);
		if (Dir) {
			LastScenarioDir = SDL_strndup(NextScenarioPath, Dir - NextScenarioPath + 1);
			if (!LastScenarioDir) BailOut("Out of memory");
			SavePrefs();
		}
	}
	Event.type = SDL_EVENT_USER;
	SDL_PushEvent(&Event);
}

Boolean ChooseLoadScenario(void)
{
	SetupFileDialog();
	SDL_ShowOpenFileDialog(ScenarioFileChosen, NULL, SdlWindow, NULL, 0, LastScenarioDir, FALSE);
	return WaitDialog();
}

static void SaveFileChosen(void *userdata, const char * const *filelist, int filter)
{
	SDL_Event Event;
	char *Dir;

	if (!filelist || !*filelist) {
		SDL_SetAtomicInt(&DialogStatus, 0);
	} else {
		SDL_SetAtomicInt(&DialogStatus, 1);
		if (SaveFileName)
			SDL_free(SaveFileName);
		SaveFileName = SDL_strdup(*filelist);
		if (!SaveFileName) BailOut("Out of memory");
		if (LastSaveDir) {
			SDL_free(LastSaveDir);
			LastSaveDir = NULL;
		}
		Dir = SDL_strrchr(SaveFileName, PATH_SEP);
		if (Dir) {
			LastSaveDir = SDL_strndup(SaveFileName, Dir - SaveFileName + 1);
			if (!LastSaveDir) BailOut("Out of memory");
			SavePrefs();
		}
	}
	Event.type = SDL_EVENT_USER;
	SDL_PushEvent(&Event);
}

Boolean ChooseLoadGame(void)
{
	SetupFileDialog();
	SDL_ShowOpenFileDialog(SaveFileChosen, NULL, SdlWindow, NULL, 0, LastSaveDir, FALSE);
	return WaitDialog();
}

/**********************************

	Show the standard file dialog
	for saving a new game

**********************************/

Boolean ChooseSaveGame(void)
{

	char *Dir;

	if (playstate != EX_STILLPLAYING && playstate != EX_AUTOMAP)
		return FALSE;
	SetupFileDialog();
	SDL_ShowSaveFileDialog(SaveFileChosen, NULL, SdlWindow, NULL, 0, LastSaveDir);
	return WaitDialog();
}

static Boolean StrToBool(const char *value)
{
	return SDL_strcasecmp(value, "true") == 0
		|| SDL_strcasecmp(value, "1") == 0;
}

static const char *const KeyPrefNames[12] = {
	"Attack", "Run", "NextWeapon", "AutoMap", "Forward", "Backward",
	"TurnLeft", "TurnRight", "Action", "Strafe", "StrafeLeft", "StrafeRight"};

static int PrefsIniHandler(void* User, const char* Section, const char* Name, const char* Value)
{
	int i;
	SDL_Scancode Code;
	if (SDL_strcasecmp(Section, "main") == 0) {
		if (SDL_strcasecmp(Name, "mouse") == 0) {
			MouseEnabled = StrToBool(Value);
		} else if (SDL_strcasecmp(Name, "governor") == 0) {
			SlowDown = StrToBool(Value);
		} else if (SDL_strcasecmp(Name, "difficulty") == 0) {
			difficulty = SDL_atoi(Value);
			if (difficulty > 3) difficulty = 3;
		}
	} else if (SDL_strcasecmp(Section, "audio") == 0) {
		if (SDL_strcasecmp(Name, "sfx") == 0) {
			if (StrToBool(Value))
				SystemState |= SfxActive;
			else
				SystemState &= ~SfxActive;
		} else if (SDL_strcasecmp(Name, "music") == 0) {
			if (StrToBool(Value))
				SystemState |= MusicActive;
			else
				SystemState &= ~MusicActive;
		}
	} else if (SDL_strcasecmp(Section, "video") == 0) {
		if (SDL_strcasecmp(Name, "fullscreen") == 0) {
			FullScreen = StrToBool(Value);
		} else if (SDL_strcasecmp(Name, "scalemode") == 0) {
			ScreenScaleMode = SDL_atoi(Value);
			if (ScreenScaleMode > 2) ScreenScaleMode = 2;
		} else if (SDL_strcasecmp(Name, "viewsize") == 0) {
			GameViewSize = SDL_atoi(Value);
			if (GameViewSize > 3) GameViewSize = 3;
		} else if (SDL_strcasecmp(Name, "filter") == 0) {
			ScreenFilter = StrToBool(Value);
		}
	} else if (SDL_strcasecmp(Section, "keys") == 0) {
		for (i = 0; i < ARRAYLEN(KeyBinds); i++) {
			if (SDL_strcasecmp(Name, KeyPrefNames[i]) == 0) {
				Code = SDL_GetScancodeFromName(Value);
				if (Code != SDL_SCANCODE_UNKNOWN)
					KeyBinds[ARRAYLEN(KeyBinds)-1-i] = Code;
			}
		}
	} else if (SDL_strcasecmp(Section, "folders") == 0) {
		if (SDL_strcasecmp(Name, "savedir") == 0) {
			SDL_free(LastSaveDir);
			LastSaveDir = SDL_strdup(Value);
		} else if (SDL_strcasecmp(Name, "loadscenariodir") == 0) {
			SDL_free(LastScenarioDir);
			LastScenarioDir = SDL_strdup(Value);
		}
	}
	return 1;
}

void LoadPrefs(void)
{
	SDL_Storage *Storage;
	char *Buf;
	Uint64 BufLen;

	SystemState = SfxActive|MusicActive;/* Assume sound & music enabled */
	MouseEnabled = FALSE;		/* Mouse control shut off */
	SlowDown = TRUE;			/* Enable speed governor */
	GameViewSize = 3;			/* 640 mode screen */
	difficulty = 2;				/* Medium difficulty */
	ScreenScaleMode = 0;
	ScreenFilter = 0;

	Storage = PrefStorage();
	if (!Storage)
		return;
	if (!SDL_GetStorageFileSize(Storage, PrefsFile, &BufLen))
		return;
	Buf = AllocSomeMem(BufLen+1);
	if (!SDL_ReadStorageFile(Storage, PrefsFile, Buf, BufLen))
		goto Done;
	Buf[BufLen] = 0;
	ini_parse_string(Buf, PrefsIniHandler, NULL);
Done:
	SDL_free(Buf);
}

/**********************************

	Save the prefs file to disk but ignore any errors

**********************************/

void SavePrefs(void)
{
	size_t BufLen;
	char *Buf;
	char *End;
	char *B;
	SDL_Storage *Storage;
	int i;

	Storage = PrefStorage();
	if (!Storage)
		return;

	BufLen = 4096;
	BufLen += LastSaveDir ? strlen(LastSaveDir) : 0;
	BufLen += LastScenarioDir ? strlen(LastScenarioDir) : 0;
	Buf = AllocSomeMem(BufLen);
	B = Buf;
	End = Buf + BufLen;

	B += snprintf(B, End - B, "[Main]\n");
	B += snprintf(B, End - B, "Mouse = %d\n", MouseEnabled);
	B += snprintf(B, End - B, "Governor = %d\n", SlowDown);
	B += snprintf(B, End - B, "Difficulty = %d\n", difficulty);
	B += snprintf(B, End - B, "\n");
	B += snprintf(B, End - B, "[Audio]\n");
	B += snprintf(B, End - B, "Sfx = %d\n", !!(SystemState & SfxActive));
	B += snprintf(B, End - B, "Music = %d\n", !!(SystemState & MusicActive));
	B += snprintf(B, End - B, "\n");
	B += snprintf(B, End - B, "[Video]\n");
	B += snprintf(B, End - B, "ViewSize = %d\n", GameViewSize);
	B += snprintf(B, End - B, "FullScreen = %d\n", FullScreen);
	B += snprintf(B, End - B, "ScaleMode = %d\n", ScreenScaleMode);
	B += snprintf(B, End - B, "Filter = %d\n", ScreenFilter);
	B += snprintf(B, End - B, "\n");
	B += snprintf(B, End - B, "[Keys]\n");
	for (i = 0; i < ARRAYLEN(KeyBinds); i++)
		B += snprintf(B, End - B, "%s = %s\n", KeyPrefNames[i], SDL_GetScancodeName(KeyBinds[11-i]));
	B += snprintf(B, End - B, "\n");
	B += snprintf(B, End - B, "[Folders]\n");
	if (LastSaveDir)
		B += snprintf(B, End - B, "SaveDir = %s\n", LastSaveDir);
	if (LastScenarioDir)
		B += snprintf(B, End - B, "LoadScenarioDir = %s\n", LastScenarioDir);
	SDL_WriteStorageFile(Storage, PrefsFile, Buf, B - Buf);
	FreeSomeMem(Buf);
}

/**********************************

	Audio mixer

**********************************/

static void ProcessMusic(void *User, SDL_AudioStream *Stream, int Needed, int Total)
{
	int FramesLeft, m, n, i;
	float *Buf;

	FramesLeft = Needed / (2 * sizeof(float));
	while (FramesLeft > 0) {
		m = SDL_min(FramesLeft, MusicBufferFrames);
		Buf = MusicBuffer;
		for (i = 64, n = m; n > 0; n -= i, Buf += i*2) {
			if (i > n) i = n;
			for (SongMSecs += i * (1000.0 / 44100.0); SongCurrent && SongMSecs >= SongCurrent->time;) {
				switch (SongCurrent->type) {
					case TML_PROGRAM_CHANGE:
						tsf_channel_set_presetnumber(MusicSynth, SongCurrent->channel, SongCurrent->program, (SongCurrent->channel == 9));
						break;
					case TML_NOTE_ON:
						tsf_channel_note_on(MusicSynth, SongCurrent->channel, SongCurrent->key, SongCurrent->velocity / 127.0f);
						break;
					case TML_NOTE_OFF:
						tsf_channel_note_off(MusicSynth, SongCurrent->channel, SongCurrent->key);
						break;
					case TML_PITCH_BEND:
						tsf_channel_set_pitchwheel(MusicSynth, SongCurrent->channel, SongCurrent->pitch_bend);
						break;
					case TML_CONTROL_CHANGE:
						tsf_channel_midi_control(MusicSynth, SongCurrent->channel, SongCurrent->control, SongCurrent->control_value);
						break;
				}
				if (SongCurrent->next) {
					SongCurrent = SongCurrent->next;
				} else {
					SongMSecs -= SongCurrent->time;
					SongCurrent = SongMidiMessages;
				}
			}
			tsf_render_float(MusicSynth, Buf, i, 0);
		}
		SDL_PutAudioStreamData(Stream, MusicBuffer, m * (2 * sizeof(float)));
		FramesLeft -= m;
	}
}

static Boolean ChangeAudioDevice(SDL_AudioDeviceID ID, const SDL_AudioSpec *Fmt)
{
	SDL_AudioSpec RealFmt;
	int BufSize;
	static const SDL_AudioSpec MacSndFmt = {
		.format = SDL_AUDIO_U8,
		.channels = 1,
		.freq = 22254,
	};
	static const SDL_AudioSpec MusicFmt = {
		.format = SDL_AUDIO_F32,
		.channels = 2,
		.freq = 44100,
	};

	if (ID == SdlAudioDevice)
		return TRUE;
	CloseAudio();

	SdlAudioDevice = SDL_OpenAudioDevice(ID, Fmt);
	if (!SdlAudioDevice)
		return FALSE;
	if (!SDL_GetAudioDeviceFormat(SdlAudioDevice, &RealFmt, &BufSize))
		goto Fail;
	SdlMusicStream = SDL_CreateAudioStream(&MusicFmt, &RealFmt);
	if (!SdlMusicStream)
		goto Fail;
	SDL_SetAudioStreamGain(SdlMusicStream, 0.5);
	if (MusicSynth) {
		MusicBufferFrames = (LongWord)BufSize * 2; 		/* Add extra overflow space on the end */
		MusicBuffer = AllocSomeMem(MusicBufferFrames * 2 * sizeof(float));
	}
	for (int i = 0; i < NAUDIOCHANS; i++) {
		SdlSfxNums[i] = -1;
		SdlSfxChannels[i] = SDL_CreateAudioStream(&MacSndFmt, &RealFmt);
		if (!SdlSfxChannels[i])
			goto Fail;
		SDL_SetAudioStreamGain(SdlSfxChannels[i], 0.75);
	}

	if (!SDL_BindAudioStream(SdlAudioDevice, SdlMusicStream))
		goto Fail;
	if (!SDL_BindAudioStreams(SdlAudioDevice, SdlSfxChannels, NAUDIOCHANS))
		goto Fail;
	if (!SDL_ResumeAudioDevice(SdlAudioDevice))
		goto Fail;
	return TRUE;
Fail:
	CloseAudio();
	return FALSE;
}

static void CloseAudio(void)
{
	if (SdlAudioDevice) {
		SDL_PauseAudioDevice(SdlAudioDevice);
		if (SdlMusicStream) {
			SDL_DestroyAudioStream(SdlMusicStream);
			SdlMusicStream = NULL;
		}
		for (int i = 0; i < NAUDIOCHANS; i++) {
			if (SdlSfxChannels[i])
				SDL_DestroyAudioStream(SdlSfxChannels[i]);
			SdlSfxChannels[i] = NULL;
			SdlSfxNums[i] = -1;
		}
		SDL_CloseAudioDevice(SdlAudioDevice);
		SdlAudioDevice = 0;
	}
	if (MusicBuffer) {
		SDL_free(MusicBuffer);
		MusicBuffer = NULL;
		MusicBufferFrames = 0;
	}
}

void PauseSoundMusicSystem(void)
{
	if (!SdlAudioDevice)
		return;
	SDL_PauseAudioDevice(SdlAudioDevice);
	EndAllSound();
}

void ResumeSoundMusicSystem(void)
{
	if (!SdlAudioDevice)
		return;
	SDL_ResumeAudioDevice(SdlAudioDevice);
}

void BeginSongLooped(Word Song)
{
	Byte *Data;
	LongWord Len;

	if (!MusicSynth)
		return;
	if (SongMidiMessages) {
		SDL_SetAudioStreamGetCallback(SdlMusicStream, NULL, NULL);
		tml_free(SongMidiMessages);
		SongMidiMessages = NULL;
	}
	tsf_reset(MusicSynth);
	Data = LoadSong(Song, &Len);
	if (!Data)
		return;
	SongMidiMessages = tml_load_memory(Data, Len);
	if (SongMidiMessages) {
		SongMSecs = 0;
		SongCurrent = SongMidiMessages;
		SDL_SetAudioStreamGetCallback(SdlMusicStream, ProcessMusic, NULL);
	}
	FreeSomeMem(Data);
}

void EndSong(void)
{
	if (!SongMidiMessages || !SdlAudioDevice)
		return;
	SDL_SetAudioStreamGetCallback(SdlMusicStream, NULL, NULL);
	tml_free(SongMidiMessages);
	tsf_reset(MusicSynth);
	SongMidiMessages = NULL;
}

void BeginSound(Word SoundNum)
{
	int i, Samples, Chan = 0, MinSamples = 0x7FFFFFFF;
	Sound *Snd;

	if (!SdlAudioDevice || SDL_AudioDevicePaused(SdlAudioDevice))
		return;
	Snd = LoadCachedSound(SoundNum);
	if (!Snd)
		return;
	for (i = 0; i < NAUDIOCHANS; i++) {
		if (SdlSfxNums[i] == (Word)-1) {
			Chan = i;
			break;
		}
		Samples = SDL_GetAudioStreamQueued(SdlSfxChannels[i]);
		if (Samples == 0) {
			Chan = i;
			break;
		}
		if (Samples < MinSamples) {
			MinSamples = Samples;
			Chan = i;
		}
	}
	SDL_ClearAudioStream(SdlSfxChannels[Chan]);
	SdlSfxNums[Chan] = SoundNum;
	SDL_SetAudioStreamFormat(SdlSfxChannels[Chan], &(SDL_AudioSpec){ SDL_AUDIO_U8, 1, Snd->samplerate }, NULL);
	SDL_PutAudioStreamData(SdlSfxChannels[Chan], Snd->data, Snd->size);
}

void EndSound(Word SoundNum)
{
	if (!SdlAudioDevice)
		return;
	for (int i = 0; i < NAUDIOCHANS; i++) {
		if (SdlSfxNums[i] == SoundNum) {
			SDL_ClearAudioStream(SdlSfxChannels[i]);
			SdlSfxNums[i] = -1;
		}
	}
}

void EndAllSound(void)
{

	if (!SdlAudioDevice)
		return;
	for (int i = 0; i < NAUDIOCHANS; i++) {
		SDL_ClearAudioStream(SdlSfxChannels[i]);
		SdlSfxNums[i] = -1;
	}
}
