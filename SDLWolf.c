#include "SDLWolf.h"
#include "Burger.h"
#include "WolfDef.h"
#include "ini.h"
#include <stdlib.h>
#include <fluidsynth.h>
#include <err.h>

static const char *PrefOrg = NULL;
static const char *PrefApp = "macwolfsdl";
static const char *PrefsFile = "macwolfsdl.ini";

#define NAUDIOCHANS 5

Word MacWidth = 0;				/* Width of play screen (Same as GameRect.right) */
Word MacHeight = 0;				/* Height of play screen (Same as GameRect.bottom) */
Word MacViewHeight;				/* Height of 3d screen (Bottom of 3D view */
static const Word MonitorWidth=640;		/* Width of the monitor in pixels */
static const Word MonitorHeight=480;		/* Height of the monitor in pixels */
static const Word VidXs[] = {320,512,640,640};	/* Screen sizes to play with */
static const Word VidYs[] = {200,384,400,480};
static const Word VidVs[] = {160,320,320,400};
static const Word VidPics[] = {rFaceShapes,rFace512,rFace640,rFace640};		/* Resource #'s for art */
static SDL_Window *SdlWindow = NULL;
SDL_Renderer *SdlRenderer = NULL;
static SDL_Texture *SdlTexture = NULL;
static SDL_Surface *SdlSurface = NULL;
static SDL_Texture *UITexture = NULL;
static SDL_Surface *UIOverlay = NULL;
static SDL_Palette *SdlPalette = NULL;
static SDL_AudioDeviceID SdlAudioDevice = 0;
static SDL_AudioStream *SdlSfxChannels[NAUDIOCHANS] = { NULL };
static SDL_AudioStream *SdlMusicStream;
static fluid_settings_t *FluidSettings;
fluid_synth_t *FluidSynth;
static fluid_player_t *FluidPlayer;
static Byte *MusicBuffer = NULL;
static LongWord MusicBufferFrames = 0;
static Word SdlSfxNums[NAUDIOCHANS];
static Byte *GameShapeBuffer = NULL;
static char *MyPrefPath = NULL;
extern int SelectedMenu;
char *SaveFileName = NULL;

static void CloseAudio(void);
static Boolean ChangeAudioDevice(SDL_AudioDeviceID ID, const SDL_AudioSpec *Fmt);
void MacLoadSoundFont(void);
static void ProcessMusic(void *User, SDL_AudioStream *Stream, int Needed, int Total);

void InitTools(void)
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO))
		errx(1, "SDL_Init");
	SDL_SetAppMetadata("Wolfenstein 3D", "1.0", "wolf3dsnes");
	LoadPrefs();
	fluid_set_log_function(FLUID_PANIC, BailOut, NULL);
	fluid_set_log_function(FLUID_ERR, NULL, NULL);
	fluid_set_log_function(FLUID_WARN, NULL, NULL);
	fluid_set_log_function(FLUID_INFO, NULL, NULL);
	FluidSettings = new_fluid_settings();
	if (FluidSettings) {
		FluidSynth = new_fluid_synth(FluidSettings);
	}
	ChangeAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
	InitResources();
	GetTableMemory();
	MacLoadSoundFont();
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
	if (!MyPrefPath) errx(1, "SDL_GetPrefPath: %s", SDL_GetError());
	return MyPrefPath;
}

SDL_Storage *PrefStorage(void)
{
	SDL_Storage *Storage;

	Storage = SDL_OpenUserStorage(PrefOrg, PrefApp, 0);
	if (!Storage)
		return NULL;
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
	SetPalette(SdlPalette, PalPtr);
}

void GoodBye()
{
	CloseAudio();
	if (FluidPlayer)
		delete_fluid_player(FluidPlayer);
	if (FluidSynth)
		delete_fluid_synth(FluidSynth);
	if (FluidSettings)
		delete_fluid_settings(FluidSettings);
	if (SdlSurface)
		SDL_DestroySurface(SdlSurface);
	if (UIOverlay)
		SDL_DestroySurface(UIOverlay);
	if (UITexture)
		SDL_DestroyTexture(UITexture);
	if (SdlTexture)
		SDL_DestroyTexture(SdlTexture);
	if (SdlRenderer)
		SDL_DestroyRenderer(SdlRenderer);
	if (SdlWindow)
		SDL_DestroyWindow(SdlWindow);
	if (MyPrefPath)
		SDL_free(MyPrefPath);
	KillResources();

	SDL_Quit();
	if (SaveFileName)
		SDL_free(SaveFileName);
	exit(0);
}

void BailOut()
{
	GoodBye();				/* Bail out! */
}

static void BlitSurfaceTex(SDL_Surface *Surface, SDL_Texture *Texture, const SDL_Rect *rect, Boolean Clear)
{
	SDL_Surface *Screen;
	Boolean Lock = (Surface->flags & SDL_SURFACE_LOCKED) != 0;
	SDL_LockTextureToSurface(Texture, NULL, &Screen);
	if (Lock)
		SDL_UnlockSurface(Surface);
	if (Clear)
		SDL_ClearSurface(Screen, 0, 0, 0, 0);
	SDL_BlitSurface(Surface, NULL, Screen, rect);
	if (Lock)
		SDL_LockSurface(Surface);
	SDL_UnlockTexture(Texture);
}

void BlitSurface(SDL_Surface *Surface, const SDL_Rect *rect)
{
	BlitSurfaceTex(Surface, SdlTexture, rect, FALSE);
}

void BlitScreen(void)
{
	BlitSurface(SdlSurface, NULL);
}

void StartUIOverlay(void)
{
	SDL_RenderTexture(SdlRenderer, SdlTexture, NULL, NULL);
	VideoPointer = UIOverlay->pixels;
	ClearTheScreen(WHITE);
}

void EndUIOverlay(void)
{
	VideoPointer = SdlSurface->pixels;
	BlitSurfaceTex(UIOverlay, UITexture, NULL, TRUE);
	SDL_RenderTexture(SdlRenderer, UITexture, NULL, NULL);
	SDL_RenderPresent(SdlRenderer);
}

void BlastScreen(void)
{
	BlitScreen();
	SDL_RenderTexture(SdlRenderer, SdlTexture, NULL, NULL);
	SDL_RenderPresent(SdlRenderer);
}

void BlastScreen2(Rect *BlastRect)
{
	BlastScreen();
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

static const Keys2Joy MenuKeyMatrix[] = {
	{SDL_SCANCODE_UP,JOYPAD_UP},				/* Arrow up */
	{SDL_SCANCODE_DOWN,JOYPAD_DN},				/* Arrow down */
	{SDL_SCANCODE_LEFT,JOYPAD_LFT},				/* Arrow Left */
	{SDL_SCANCODE_RIGHT,JOYPAD_RGT},				/* Arrow Right */
	{ SDL_SCANCODE_SPACE,JOYPAD_A},				/* Space */
	{ SDL_SCANCODE_RETURN,JOYPAD_A},				/* Return */
	{ SDL_SCANCODE_KP_ENTER,JOYPAD_A},				/* keypad enter */
	{ SDL_SCANCODE_ESCAPE,JOYPAD_B},				/* keypad enter */
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
	const SDL_Keycode *KeyPtr = KeyBinds;

	joystick1 = 0;			/* Assume that joystick not moved */
	mousewheel = 0;

	/* Switch weapons like in DOOM! */
	PauseExited = FALSE;
	SDL_PumpEvents();
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			GoodBye();
		} else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			mousewheel += event.wheel.y;
		} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			mousebuttons |= SDL_BUTTON_MASK(event.button.button);
		} else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
			mousebuttons &= ~SDL_BUTTON_MASK(event.button.button);
		} else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_ESCAPE) {
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
	} while (++i<sizeof KeyBinds/sizeof *KeyBinds);				/* All done? */
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

	joystick1 = 0;
	mousewheel = 0;

	SDL_PumpEvents();
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			GoodBye();
		} else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			mousewheel += event.wheel.y;
		} else if (event.type == SDL_EVENT_MOUSE_MOTION) {
			mousex = event.motion.x;
			mousey = event.motion.y;
		} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			mousex = event.motion.x;
			mousey = event.motion.y;
			return event.button.button;
		} else if (event.type == SDL_EVENT_KEY_DOWN) {
			KeyPtr = MenuKeyMatrix;
			for (i = 0; i < sizeof MenuKeyMatrix/sizeof *MenuKeyMatrix; i++) {
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

void ResizeGameWindow(Word Width, Word Height)
{
	SDL_Palette *Palette;

	if (Width == MacWidth && Height == MacHeight)
		return;
	if ((Width != MacWidth || Height != MacHeight) && MathSize != (Word)-1)
		MathSize = -2;
	MacWidth = Width;
	MacHeight = Height;
	MacViewHeight = Height;
	MacVidSize = -1;
	if (!SdlWindow) {
		SdlWindow = SDL_CreateWindow("Wolfenstein 3D", MacWidth, MacHeight, 0);
		if (!SdlWindow)
			errx(1, "SDL_CreateWindow");
		SdlRenderer = SDL_CreateRenderer(SdlWindow, NULL);
		if (!SdlRenderer)
			errx(1, "SDL_CreateRenderer");
	} else {
		if (SdlSurface)
			SDL_DestroySurface(SdlSurface);
		if (UIOverlay)
			SDL_DestroySurface(UIOverlay);
		if (SdlTexture)
			SDL_DestroyTexture(SdlTexture);
		if (UITexture)
			SDL_DestroyTexture(UITexture);
		SDL_SetWindowSize(SdlWindow, MacWidth, MacHeight);
	}
	SdlSurface = SDL_CreateSurface(MacWidth, MacHeight, SDL_PIXELFORMAT_INDEX8);
	if (!SdlSurface)
		errx(1, "SDL_CreateSurface");
	SdlPalette = SDL_CreateSurfacePalette(SdlSurface);
	if (!SdlPalette)
		errx(1, "SDL_CreateSurfacePalette");
	UIOverlay = SDL_CreateSurface(MacWidth, MacHeight, SDL_PIXELFORMAT_INDEX8);
	if (!UIOverlay)
		errx(1, "SDL_CreateSurface");
	Palette = SDL_CreateSurfacePalette(UIOverlay);
	if (!Palette)
		errx(1, "SDL_CreateSurfacePalette");
	SetPalette(Palette, LoadAResource(rGamePal));
	ReleaseAResource(rGamePal);
	SDL_SetSurfaceBlendMode(UIOverlay, SDL_BLENDMODE_NONE);
	SDL_SetSurfaceColorKey(UIOverlay, TRUE, 0);
	SdlTexture = SDL_CreateTexture(SdlRenderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING, MacWidth, MacHeight);
	if (!SdlTexture)
		errx(1, "SDL_CreateTexture");
	UITexture = SDL_CreateTexture(SdlRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, MacWidth, MacHeight);
	if (!UITexture)
		errx(1, "SDL_CreateTexture");

	if (SDL_MUSTLOCK(SdlSurface))
		SDL_LockSurface(SdlSurface);
	if (SDL_MUSTLOCK(UIOverlay))
		SDL_LockSurface(UIOverlay);
	VideoPointer = SdlSurface->pixels;
	VideoWidth = SdlSurface->pitch;
	InitYTable();				/* Init the game's YTable */
}

Word NewGameWindow(Word NewVidSize)
{
	LongWord *LongPtr;
	LongWord PackLength;
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

	LongPtr = (LongWord *) LoadAResource(VidPics[MacVidSize]);
	if (!LongPtr) {
		goto OhShit;
	}
	PackLength = SwapLongBE(LongPtr[0]);
	GameShapeBuffer = AllocSomeMem(PackLength);	/* All the permanent game shapes */
	if (!GameShapeBuffer) {		/* Can't load in the shapes */
		ReleaseAResource(VidPics[MacVidSize]);	/* Release it NOW! */
		goto OhShit;
	}
	DLZSS(GameShapeBuffer,(Byte *) &LongPtr[1], PackLength);
	ReleaseAResource(VidPics[MacVidSize]);
	i = 0;
	j = (MacVidSize==1) ? 47+10 : 47;		/* 512 mode has 10 shapes more */
	LongPtr = (LongWord *) GameShapeBuffer;
	do {
		GameShapes[i] = GameShapeBuffer+SwapLongBE(LongPtr[i]);
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
			BailOut();
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
	LongWord *PackPtr;
	Byte *ShapePtr;
	LongWord PackLength;
	Word X,Y;

	ClearTheScreen(BLACK);
	BlastScreen();
	PackPtr = LoadAResource(rGetPsychPic);
	PackLength = SwapLongBE(PackPtr[0]);
	ShapePtr = AllocSomeMem(PackLength);
	DLZSS(ShapePtr,(Byte *) &PackPtr[1],PackLength);

	X = (MacWidth-224)/2;
	Y = (MacViewHeight-56)/2;
	DrawShape(X,Y,ShapePtr);
	FreeSomeMem(ShapePtr);
	ReleaseAResource(rGetPsychPic);
	BlastScreen();
	SetAPalette(rGamePal);
	SDL_SetRenderDrawColor(SdlRenderer, 255, 0, 0, 255);
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
	SDL_RenderTexture(SdlRenderer, SdlTexture, NULL, NULL);
	SDL_RenderFillRect(SdlRenderer, &PsychedRect);
	SDL_RenderPresent(SdlRenderer);
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

void SaveGame(void)
{
	long Count;
	Word PWallWord;
	FILE *FileRef;

	if (!SaveFileName)
		return;
	FileRef = fopen(SaveFileName, "wb");
	if (!FileRef)
		return;

	Count = 4;							/* Default length */
	fwrite(&MachType, 1, Count, FileRef);	/* Save a machine type ID */
	Count = sizeof(unsigned short);
	fwrite(&MapListPtr->MaxMap, 1, Count, FileRef); 	/* Number of maps (ID) */
	Count = sizeof(gamestate);
	fwrite(&gamestate, 1, Count, FileRef);		/* Save the game stats */
	Count = sizeof(PushWallRec);
	fwrite(&PushWallRec, 1, Count, FileRef);	/* Save the pushwall stats */

	Count = sizeof(nummissiles);
	fwrite(&nummissiles, 1, Count, FileRef);	/* Save missiles in motion */
	if (nummissiles) {
		Count = nummissiles*sizeof(missile_t);
		fwrite(&missiles[0], 1, Count, FileRef);
	}
	Count = sizeof(numactors);
	fwrite(&numactors, 1, Count, FileRef);		/* Save actors */
	if (numactors) {
		Count = numactors*sizeof(actor_t);
		fwrite(&actors[0], 1, Count, FileRef);
	}
	Count = sizeof(numdoors);
	fwrite(&numdoors, 1, Count, FileRef);		/* Save doors */
	if (numdoors) {
		Count = numdoors*sizeof(door_t);
		fwrite(&doors[0], 1, Count, FileRef);
	}
	Count = sizeof(numstatics);
	fwrite(&numstatics, 1, Count, FileRef);
	if (numstatics) {
		Count = numstatics*sizeof(static_t);
		fwrite(&statics[0], 1, Count, FileRef);
	}
	Count = 64*64;
	fwrite(MapPtr, 1, Count, FileRef);
	Count = sizeof(tilemap);				/* Tile map */
	fwrite(&tilemap, 1, Count, FileRef);

	Count = sizeof(ConnectCount);
	fwrite(&ConnectCount, 1, Count, FileRef);
	if (ConnectCount) {
		Count = ConnectCount * sizeof(connect_t);
		fwrite(&areaconnect[0], 1, Count, FileRef);
	}
	Count = sizeof(areabyplayer);
	fwrite(&areabyplayer[0], 1, Count, FileRef);
	Count = (128+5)*64;
	fwrite(&textures[0], 1, Count, FileRef);
	Count = sizeof(Word);
	PWallWord = 0;		/* Assume no pushwall pointer in progress */
	if (pwallseg) {
		PWallWord = (pwallseg-(saveseg_t*)nodes)+1;		/* Convert to number offset */
	}
	fwrite(&PWallWord, 1, Count, FileRef);
	Count = MapPtr->numnodes*sizeof(savenode_t);	/* How large is the BSP tree? */
	fwrite(nodes, 1, Count, FileRef);			/* Save it to disk */
	fclose(FileRef);						/* Close the file */
	PlaySound(SND_BONUS);
}

/**********************************

	Load the game

**********************************/
void *SaveRecord;
void *SaveRecordMem;

Boolean LoadGame(void)
{
	LongWord FileSize;
	union {
		Byte *B;
		unsigned short *S;
		Word *W;
		LongWord *L;
	} FilePtr;
	void *TheMem;
	FILE *FileRef;

	if (!SaveFileName)
		return FALSE;
	FileRef = fopen(SaveFileName, "wb");
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
	if (!FilePtr.B) {						/* No memory! */
		return FALSE;
	}
	fread(FilePtr.B, 1, FileSize, FileRef);	/* Open the file */
	fclose(FileRef);						/* Close the file */
	if (memcmp(MachType,FilePtr.B,4)) {		/* Is this the proper machine type? */
		goto Bogus;
	}
	FilePtr.B+=4;							/* Index past signature */
	if (MapListPtr->MaxMap!=*FilePtr.S) {	/* Map count the same? */
		goto Bogus;							/* Must be differant game */
	}
	++FilePtr.S;							/* Index past count */
	memcpy(&gamestate,FilePtr.B,sizeof(gamestate));	/* Reset the game state */
	SaveRecord = FilePtr.B;
	SaveRecordMem = TheMem;
	return TRUE;

Bogus:
	FreeSomeMem(TheMem);
BadFile:
	fclose(FileRef);
	return FALSE;
}

void FinishLoadGame(void)
{
	union {
		Byte *B;
		unsigned short *S;
		Word *W;
		LongWord *L;
	} FilePtr;

	FilePtr.B = SaveRecord;

	memcpy(&gamestate,FilePtr.B,sizeof(gamestate));	/* Reset the game state */
	FilePtr.B+=sizeof(gamestate);

	memcpy(&PushWallRec,FilePtr.B,sizeof(PushWallRec));				/* Record for the single pushwall in progress */
	FilePtr.B+=sizeof(PushWallRec);

	nummissiles = *FilePtr.W;
	++FilePtr.W;
	if (nummissiles) {
		memcpy(&missiles[0],FilePtr.B,sizeof(missile_t)*nummissiles);
		FilePtr.B += sizeof(missile_t)*nummissiles;
	}

	numactors = *FilePtr.W;
	++FilePtr.W;
	if (numactors) {
		memcpy(&actors[0],FilePtr.B,sizeof(actor_t)*numactors);
		FilePtr.B += sizeof(actor_t)*numactors;
	}

	numdoors = *FilePtr.W;
	++FilePtr.W;
	if (numdoors) {
		memcpy(&doors[0],FilePtr.B,sizeof(door_t)*numdoors);
		FilePtr.B += sizeof(door_t)*numdoors;
	}

	numstatics = *FilePtr.W;
	++FilePtr.W;
	if (numstatics) {
		memcpy(&statics[0],FilePtr.B,sizeof(static_t)*numstatics);
		FilePtr.B += sizeof(static_t)*numstatics;
	}
	memcpy(MapPtr,FilePtr.B,64*64);
	FilePtr.B += 64*64;
	memcpy(&tilemap,FilePtr.B,sizeof(tilemap));						/* Tile map */
	FilePtr.B += sizeof(tilemap);		/* Index past data */

	ConnectCount = *FilePtr.W;		/* Number of valid interconnects */
	FilePtr.W++;
	if (ConnectCount) {
		memcpy(areaconnect,FilePtr.B,sizeof(connect_t)*ConnectCount);	/* Is this area mated with another? */
		FilePtr.B+= sizeof(connect_t)*ConnectCount;
	}
	memcpy(areabyplayer,FilePtr.B,sizeof(areabyplayer));
	FilePtr.B+=sizeof(areabyplayer);	/* Which areas can I see into? */

	memcpy(&textures[0],FilePtr.B,(128+5)*64);
	FilePtr.B+=((128+5)*64);				/* Texture array for pushwalls */

	pwallseg = 0;			/* Assume bogus */
	if (*FilePtr.W) {
		pwallseg = (saveseg_t *)nodes;
		pwallseg = &pwallseg[*FilePtr.W-1];
	}
	++FilePtr.W;
	memcpy(nodes,FilePtr.B,MapPtr->numnodes*sizeof(savenode_t));
/*	FilePtr.B+=(MapPtr->numnodes*sizeof(savenode_t));	*//* Next entry */

	FreeSomeMem(SaveRecordMem);
}

/**********************************

	Show the standard file dialog
	for opening a new game

**********************************/

static SDL_Mutex *DialogMutex = NULL;
static Boolean DialogCancel = FALSE;

static void SaveFileChosen(void *userdata, const char * const *filelist, int filter)
{
	if (!filelist || !*filelist) {
		DialogCancel = TRUE;
	} else {
		DialogCancel = FALSE;
		if (SaveFileName)
			SDL_free(SaveFileName);
		SaveFileName = SDL_strdup(*filelist);
	}
	SDL_UnlockMutex(DialogMutex);
}

Boolean ChooseLoadGame(void)
{
	if (!DialogMutex)
		DialogMutex = SDL_CreateMutex();
	SDL_LockMutex(DialogMutex);
	SDL_ShowOpenFileDialog(SaveFileChosen, NULL, SdlWindow, NULL, 0, NULL, FALSE);
	SDL_LockMutex(DialogMutex);
	SDL_UnlockMutex(DialogMutex);
	return DialogCancel;
}

/**********************************

	Show the standard file dialog
	for saving a new game

**********************************/

Boolean ChooseSaveGame(void)
{
	if (!DialogMutex)
		DialogMutex = SDL_CreateMutex();
	SDL_LockMutex(DialogMutex);
	SDL_ShowSaveFileDialog(SaveFileChosen, NULL, SdlWindow, NULL, 0, NULL);
	SDL_LockMutex(DialogMutex);
	SDL_UnlockMutex(DialogMutex);
	return DialogCancel;
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
		if (SDL_strcasecmp(Name, "state") == 0) {
			SystemState = SDL_atoi(Value) & 3;
		} else if (SDL_strcasecmp(Name, "mouse") == 0) {
			MouseEnabled = StrToBool(Value);
		} else if (SDL_strcasecmp(Name, "governor") == 0) {
			SlowDown = StrToBool(Value);
		} else if (SDL_strcasecmp(Name, "viewsize") == 0) {
			GameViewSize = SDL_atoi(Value);
			if (GameViewSize > 3) GameViewSize = 3;
		} else if (SDL_strcasecmp(Name, "difficulty") == 0) {
			difficulty = SDL_atoi(Value);
			if (difficulty > 3) difficulty = 3;
		}
	} else if (SDL_strcasecmp(Section, "keys") == 0) {
		for (i = 0; i < 12; i++) {
			if (SDL_strcasecmp(Name, KeyPrefNames[i]) == 0) {
				Code = SDL_GetScancodeFromName(Value);
				if (Code != SDL_SCANCODE_UNKNOWN)
					KeyBinds[11-i] = Code;
			}
		}
	}
	return 1;
}

void LoadPrefs(void)
{
	SDL_Storage *Storage;
	char *Buf;
	Uint64 BufLen;

	SystemState = 3;			/* Assume sound & music enabled */
	MouseEnabled = FALSE;		/* Mouse control shut off */
	SlowDown = TRUE;			/* Enable speed governor */
	GameViewSize = 3;			/* 640 mode screen */
	difficulty = 2;				/* Medium difficulty */

	Storage = PrefStorage();
	if (!Storage)
		return;
	if (!SDL_GetStorageFileSize(Storage, PrefsFile, &BufLen))
		return;
	Buf = SDL_malloc(BufLen+1);
	if (!Buf) err(1, "malloc");
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
	char Buf[4096];
	char * const End = Buf + sizeof Buf;
	char *B = Buf;
	SDL_Storage *Storage;
	int i;

	Storage = PrefStorage();
	if (!Storage)
		return;
	B += snprintf(B, End - B, "[Main]\n");
	B += snprintf(B, End - B, "State = %d\n", SystemState);
	B += snprintf(B, End - B, "Mouse = %d\n", MouseEnabled);
	B += snprintf(B, End - B, "Governor = %d\n", SlowDown);
	B += snprintf(B, End - B, "ViewSize = %d\n", GameViewSize);
	B += snprintf(B, End - B, "Difficulty = %d\n", difficulty);
	B += snprintf(B, End - B, "[Keys]\n");
	for (i = 0; i < 12; i++)
		B += snprintf(B, End - B, "%s = %s\n", KeyPrefNames[i], SDL_GetScancodeName(KeyBinds[11-i]));
	SDL_WriteStorageFile(Storage, PrefsFile, Buf, B - Buf);
}

/**********************************

	Audio mixer

**********************************/

static void ProcessMusic(void *User, SDL_AudioStream *Stream, int Needed, int Total)
{
	int FramesLeft, n;

	FramesLeft = (Needed + (2 * sizeof(int16_t))-1) / (2 * sizeof(int16_t));
	while (FramesLeft > 0) {
		n = FramesLeft < MusicBufferFrames ? FramesLeft : MusicBufferFrames;
		fluid_synth_write_s16(FluidSynth, n, MusicBuffer, 0, 2, MusicBuffer, 1, 2);
		SDL_PutAudioStreamData(Stream, MusicBuffer, n * (2 * sizeof(int16_t)));
		FramesLeft -= n;
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
		.format = SDL_AUDIO_S16,
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
if (FluidSynth) {
		MusicBufferFrames = (LongWord)BufSize * 2;
		MusicBuffer = SDL_malloc(MusicBufferFrames * 2 * sizeof(int16_t));
		if (!MusicBuffer) err(1, "malloc");
	}
	for (int i = 0; i < NAUDIOCHANS; i++) {
		SdlSfxNums[i] = -1;
		SdlSfxChannels[i] = SDL_CreateAudioStream(&MacSndFmt, &RealFmt);
		if (!SdlSfxChannels[i])
			goto Fail;
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
	if (FluidPlayer)
		fluid_player_stop(FluidPlayer);
	SDL_PauseAudioDevice(SdlAudioDevice);
	EndAllSound();
}

void ResumeSoundMusicSystem(void)
{
	if (!SdlAudioDevice)
		return;
	if (FluidPlayer && fluid_player_get_status(FluidPlayer) != FLUID_PLAYER_PLAYING)
		fluid_player_play(FluidPlayer);
	SDL_ResumeAudioDevice(SdlAudioDevice);
}

void BeginSongLooped(Word Song)
{
	Byte *Data;
	LongWord Len;

	if (!FluidSynth)
		return;
	if (FluidPlayer) {
		SDL_SetAudioStreamGetCallback(SdlMusicStream, NULL, NULL);
		delete_fluid_player(FluidPlayer);
		FluidPlayer = NULL;
	}
	fluid_synth_system_reset(FluidSynth);
	Data = LoadSong(Song, &Len);
	if (!Data)
		return;
	FluidPlayer = new_fluid_player(FluidSynth);
	if (FluidPlayer) {
		if (fluid_player_add_mem(FluidPlayer, Data, Len) != FLUID_FAILED) {
			fluid_player_set_loop(FluidPlayer, -1);
			fluid_player_play(FluidPlayer);
			SDL_SetAudioStreamGetCallback(SdlMusicStream, ProcessMusic, NULL);
		}
	}
	SDL_free(Data);
}

void EndSong()
{
	if (!FluidPlayer || !SdlAudioDevice)
		return;
	SDL_SetAudioStreamGetCallback(SdlMusicStream, NULL, NULL);
	delete_fluid_player(FluidPlayer);
	fluid_synth_system_reset(FluidSynth);
	FluidPlayer = NULL;
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
