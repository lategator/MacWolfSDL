#include "Burger.h"
#include "WolfDef.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

#define NAUDIOCHANS 8

Word MacWidth;					/* Width of play screen (Same as GameRect.right) */
Word MacHeight;					/* Height of play screen (Same as GameRect.bottom) */
Word MacViewHeight;				/* Height of 3d screen (Bottom of 3D view */
static const Word MonitorWidth=640;		/* Width of the monitor in pixels */
static const Word MonitorHeight=480;		/* Height of the monitor in pixels */
static const Word VidXs[] = {320,512,640,640};	/* Screen sizes to play with */
static const Word VidYs[] = {200,384,400,480};
static const Word VidVs[] = {160,320,320,400};
static const Word VidPics[] = {rFaceShapes,rFace512,rFace640,rFace640};		/* Resource #'s for art */
static SDL_Window *SdlWindow = NULL;
static SDL_Renderer *SdlRenderer = NULL;
static SDL_Texture *SdlTexture = NULL;
static SDL_Surface *SdlSurface = NULL;
static SDL_Palette *SdlPalette = NULL;
static SDL_AudioDeviceID SdlAudioDevice = 0;
static SDL_AudioStream *SdlSfxChannels[NAUDIOCHANS] = { NULL };
static Word SdlSfxNums[NAUDIOCHANS];
static char *SaveFileName = NULL;
static Byte *GameShapeBuffer = NULL;

Boolean ChangeAudioDevice(SDL_AudioDeviceID ID, const SDL_AudioSpec *Fmt);

static Word DoMyAlert(Word AlertNum)
{
	return 0;
}

void InitTools(void)
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO))
		errx(1, "SDL_Init");
	ChangeAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
	InitResources();
	if (!LoadLevelResources())
		err(1, "LoadLevelResources");
	GetTableMemory();
	LoadMapSetData();
	if (MapListPtr->MaxMap==3) {	/* Shareware version? */
		DoMyAlert(ShareWareWin);	/* Show the shareware message */
	}
	NewGameWindow(3);				/* Create a game window at 512x384 */
	ClearTheScreen(BLACK);			/* Force the offscreen memory blank */
	BlastScreen();
}

Byte CurrentPal[768];

void SetAPalettePtr(unsigned char *PalPtr)
{
	Word i;					/* Temp */

	memcpy(CurrentPal,PalPtr,768);
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
	} while (++i<255);	/* All done? */
	SDL_SetPaletteColors(SdlPalette, Colors, 0, 256);
}

void GoodBye(void)
{
	if (SdlSurface) {
		if (SDL_MUSTLOCK(SdlSurface))
			SDL_UnlockSurface(SdlSurface);
		SDL_DestroySurface(SdlSurface);
	}
	if (SdlTexture)
		SDL_DestroyTexture(SdlTexture);
	if (SdlRenderer)
		SDL_DestroyRenderer(SdlRenderer);
	if (SdlWindow)
		SDL_DestroyWindow(SdlWindow);

	SDL_Quit();
	if (SaveFileName)
		SDL_free(SaveFileName);
	exit(0);
}

void BailOut(void)
{
	DoMyAlert(rUserAlert);		/* Show the alert window */
	GoodBye();				/* Bail out! */
}

static void BlitScreen(void)
{
	SDL_Surface *Screen;
	SDL_LockTextureToSurface(SdlTexture, NULL, &Screen);
	if (SDL_MUSTLOCK(SdlSurface))
		SDL_UnlockSurface(SdlSurface);
	SDL_BlitSurface(SdlSurface, NULL, Screen, NULL);
	if (SDL_MUSTLOCK(SdlSurface))
		SDL_LockSurface(SdlSurface);
	SDL_UnlockTexture(SdlTexture);
	SDL_RenderTexture(SdlRenderer, SdlTexture, NULL, NULL);
}

void BlastScreen(void)
{
	BlitScreen();
	SDL_RenderPresent(SdlRenderer);
}
void BlastScreen2(Rect *BlastRect)
{
  BlastScreen();
}

/**********************************

	Read from the keyboard/mouse system

**********************************/

typedef struct {
	SDL_Scancode Code;
	Word JoyValue;
} Keys2Joy;

static const Keys2Joy KeyMatrix[] = {
	{SDL_SCANCODE_KP_7,JOYPAD_LFT|JOYPAD_UP},		/* Keypad 7 */
	{SDL_SCANCODE_KP_4,JOYPAD_LFT},				/* Keypad 4 */
	{SDL_SCANCODE_KP_1,JOYPAD_LFT|JOYPAD_DN},		/* Keypad 1 */
	{SDL_SCANCODE_KP_8,JOYPAD_UP},				/* Keypad 8 */
	{SDL_SCANCODE_KP_5,JOYPAD_DN},				/* Keypad 5 */
	{SDL_SCANCODE_KP_2,JOYPAD_DN},				/* Keypad 2 */
	{SDL_SCANCODE_KP_9,JOYPAD_RGT|JOYPAD_UP},		/* Keypad 9 */
	{SDL_SCANCODE_KP_6,JOYPAD_RGT},				/* Keypad 6 */
	{SDL_SCANCODE_KP_3,JOYPAD_RGT|JOYPAD_DN},		/* Keypad 3 */
	{SDL_SCANCODE_UP,JOYPAD_UP},				/* Arrow up */
	{SDL_SCANCODE_DOWN,JOYPAD_DN},				/* Arrow down */
	{SDL_SCANCODE_LEFT,JOYPAD_LFT},				/* Arrow Left */
	{SDL_SCANCODE_RIGHT,JOYPAD_RGT},				/* Arrow Right */
	{ SDL_SCANCODE_W,JOYPAD_UP},			/* W */
	{ SDL_SCANCODE_A,JOYPAD_LFT},			/* A */
	{ SDL_SCANCODE_S,JOYPAD_DN},			/* S */
	{ SDL_SCANCODE_D,JOYPAD_RGT},			/* D */
	{ SDL_SCANCODE_I,JOYPAD_UP},			/* I */
	{ SDL_SCANCODE_J,JOYPAD_LFT},			/* J */
	{ SDL_SCANCODE_K,JOYPAD_DN},			/* K */
	{ SDL_SCANCODE_L,JOYPAD_RGT},			/* L */
	{ SDL_SCANCODE_SPACE,JOYPAD_A},				/* Space */
	{ SDL_SCANCODE_RETURN,JOYPAD_A},				/* Return */
	{SDL_SCANCODE_KP_0,JOYPAD_A},				/* Keypad 0 */
	{ SDL_SCANCODE_KP_ENTER,JOYPAD_A},				/* keypad enter */
	{ SDL_SCANCODE_RALT,JOYPAD_TR},			/* Option */
	{ SDL_SCANCODE_LALT,JOYPAD_TR},			/* Option */
	{ SDL_SCANCODE_RSHIFT,JOYPAD_X},				/* Shift L */
	{ SDL_SCANCODE_CAPSLOCK,JOYPAD_X},				/* Caps Lock */
	{ SDL_SCANCODE_LSHIFT,JOYPAD_X},				/* Shift R */
	{ SDL_SCANCODE_TAB,JOYPAD_SELECT},		/* Tab */
	{ SDL_SCANCODE_LCTRL,JOYPAD_B},				/* Ctrl */
	{ SDL_SCANCODE_RCTRL,JOYPAD_B}				/* Ctrl */
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

void ReadSystemJoystick(void)
{
	Word i;
	Word Index;
	SDL_Event event;
	SDL_MouseButtonFlags Buttons;
	const bool *Keys;
	const Keys2Joy *KeyPtr = KeyMatrix;

	joystick1 = 0;			/* Assume that joystick not moved */

	/* Switch weapons like in DOOM! */
  SDL_PumpEvents();
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			GoodBye();
		} else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
			//PauseSoundMusicSystem();	/* Pause the music */
			WaitTicksEvent(0);
			LastTicCount = ReadTick();	/* Reset the timer for the pause key */
		} else if (event.type == SDL_EVENT_KEY_DOWN) {
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
				case SDLK_PERIOD:		/* Keypad Period */
				case SDLK_SLASH:		/* Slash */
					joystick1 = JOYPAD_START;
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
	Keys = SDL_GetKeyboardState(NULL);
	i = 0;					/* Init the count */
	do {
		if (Keys[KeyPtr->Code])
			joystick1 |= KeyPtr->JoyValue;	/* Set the joystick value */
		KeyPtr++;					/* Next index */
	} while (++i<33);				/* All done? */
	if (MouseEnabled) {
		Buttons = SDL_GetMouseState(NULL, NULL);
		if (Buttons & SDL_BUTTON_LMASK)
			joystick1 |= JOYPAD_B;
		if (Buttons & SDL_BUTTON_RMASK)
			joystick1 |= JOYPAD_A;
	}

	if (joystick1 & JOYPAD_TR) {		/* Handle the side scroll (Special case) */
		if (joystick1&JOYPAD_LFT) {
			joystick1 = (joystick1 & ~(JOYPAD_TR|JOYPAD_LFT)) | JOYPAD_TL;
		} else if (joystick1&JOYPAD_RGT) {
			joystick1 = joystick1 & ~JOYPAD_RGT;
		} else {
			joystick1 &= ~JOYPAD_TR;
		}
	}
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
TryAgain:
	if (GameShapeBuffer) {
		FreeSomeMem(GameShapeBuffer);	/* All the permanent game shapes */
		GameShapeBuffer=0;
		for (i = 0; i < 57; i++)
			GameShapes[i] = NULL;
	}
	MacVidSize = NewVidSize;	/* Set the new data size */
	MacWidth = VidXs[NewVidSize];
	MacHeight = VidYs[NewVidSize];
	MacViewHeight = VidVs[NewVidSize];

	if (!SdlWindow) {
		SdlWindow = SDL_CreateWindow("Wolfenstein 3D", MacWidth, MacHeight, 0);
		if (!SdlWindow)
			errx(1, "SDL_CreateWindow");
		SdlRenderer = SDL_CreateRenderer(SdlWindow, NULL);
		if (!SdlRenderer)
			errx(1, "SDL_CreateRenderer");
	} else {
		if (SdlSurface) {
			if (SDL_MUSTLOCK(SdlSurface))
				SDL_UnlockSurface(SdlSurface);
			SDL_DestroySurface(SdlSurface);
		}
		if (SdlTexture)
			SDL_DestroyTexture(SdlTexture);
		SDL_SetWindowSize(SdlWindow, MacWidth, MacHeight);
	}
	SdlSurface = SDL_CreateSurface(MacWidth, MacHeight, SDL_PIXELFORMAT_INDEX8);
	if (!SdlSurface)
		errx(1, "SDL_CreateSurface");
	SdlPalette = SDL_CreateSurfacePalette(SdlSurface);
	if (!SdlPalette)
		errx(1, "SDL_CreateSurfacePalette");
	SdlTexture = SDL_CreateTexture(SdlRenderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING, MacWidth, MacHeight);
	if (!SdlTexture)
		errx(1, "SDL_CreateTexture");

	if (SDL_MUSTLOCK(SdlSurface))
		SDL_LockSurface(SdlSurface);
	VideoPointer = SdlSurface->pixels;
	VideoWidth = SdlSurface->pitch;
	InitYTable();				/* Init the game's YTable */
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
	SDL_RenderFillRect(SdlRenderer, &PsychedRect);
	SDL_RenderPresent(SdlRenderer);
}

void EndGetPsyched(void)
{
	SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
	SetAPalette(rBlackPal);		/* Zap the palette */
}

Word ChooseGameDiff(void)
{
	difficulty = 1;
	return TRUE;
}

void ShareWareEnd(void)
{
	SetAPalette(rGamePal);
	DoMyAlert(EndGameWin);
	SetAPalette(rBlackPal);
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

/**********************************

	Audio mixer

**********************************/

static void CloseAudio(void);

Boolean ChangeAudioDevice(SDL_AudioDeviceID ID, const SDL_AudioSpec *Fmt)
{
	SDL_AudioSpec RealFmt;
	static const SDL_AudioSpec MacSndFmt = {
		.format = SDL_AUDIO_U8,
		.channels = 1,
		.freq = 22254,
	};

	if (ID == SdlAudioDevice)
		return TRUE;
	CloseAudio();

	SdlAudioDevice = SDL_OpenAudioDevice(ID, Fmt);
	if (!SdlAudioDevice)
		return FALSE;
	if (!SDL_GetAudioDeviceFormat(SdlAudioDevice, &RealFmt, NULL))
		goto Fail;
	for (int i = 0; i < NAUDIOCHANS; i++) {
		SdlSfxNums[i] = -1;
		SdlSfxChannels[i] = SDL_CreateAudioStream(&MacSndFmt, &RealFmt);
		if (!SdlSfxChannels[i])
			goto Fail;
	}

	if (!SDL_BindAudioStreams(SdlAudioDevice, SdlSfxChannels, NAUDIOCHANS))
		goto Fail;
	if (!SDL_ResumeAudioDevice(SdlAudioDevice))
		goto Fail;
	return TRUE;
Fail:
	for (int i = 0; i < NAUDIOCHANS; i++) {
		if (SdlSfxChannels[i])
			SDL_DestroyAudioStream(SdlSfxChannels[i]);
	}
	SDL_CloseAudioDevice(SdlAudioDevice);
	return FALSE;
}

static void CloseAudio(void)
{
	if (SdlAudioDevice) {
		SDL_PauseAudioDevice(SdlAudioDevice);
		for (int i = 0; i < NAUDIOCHANS; i++) {
			if (SdlSfxChannels[i])
				SDL_DestroyAudioStream(SdlSfxChannels[i]);
			SdlSfxChannels[i] = NULL;
			SdlSfxNums[i] = -1;
		}
		SDL_CloseAudioDevice(SdlAudioDevice);
		SdlAudioDevice = 0;
	}
}

void BeginSound(Word SoundNum) {
	int i, Samples, Chan = 0, MinSamples = 0x7FFFFFFF;
	Sound *Snd;

	if (!SdlAudioDevice || SDL_AudioDevicePaused(SdlAudioDevice))
		return;
	Snd = LoadSound(SoundNum);
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
	SDL_PutAudioStreamData(SdlSfxChannels[Chan], Snd->data, Snd->size);
}
void EndSound(Word SoundNum) {
	for (int i = 0; i < NAUDIOCHANS; i++) {
		if (SdlSfxNums[i] == SoundNum) {
			SDL_ClearAudioStream(SdlSfxChannels[i]);
			SdlSfxNums[i] = -1;
		}
	}
}
void EndAllSound(void) {
	for (int i = 0; i < NAUDIOCHANS; i++) {
		SDL_ClearAudioStream(SdlSfxChannels[i]);
		SdlSfxNums[i] = -1;
	}
}
