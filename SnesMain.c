#include "WolfDef.h"
#include <SDL3/SDL_main.h>

/**********************************

	Load the map tables

**********************************/

void LoadMapSetData(void)
{
	LongWord Length;
	int i;
	maplist_t *Maps;
	short *Sounds;
	unsigned short *Songs;
	unsigned short *Walls;

	Sounds = LoadAResource(MySoundList);	/* Get the list of sounds */
	Maps = LoadAResource(rMapList);	/* Get the map list */
	Songs = LoadAResource(rSongList);
	Walls = LoadAResource(MyWallList);
	if (Sounds != SoundListPtr) {
		Length = ResourceLength(MySoundList)>>1;
		for (i = 0; i < Length; i++)
			Sounds[i] = SwapShortBE(Sounds[i]);
		RegisterSounds(Sounds);
		SoundListPtr = Sounds;
	}
	if (Maps != MapListPtr) {
		if (Maps) {
			Maps->MaxMap = SwapUShortBE(Maps->MaxMap);
			Maps->MapRezNum = SwapUShortBE(Maps->MapRezNum);
			for (int i = 0; i < Maps->MaxMap; i++) {
				MapInfo_t *Info = &Maps->InfoArray[i];
				Info->NextLevel = SwapUShortBE(Info->NextLevel);
				Info->SecretLevel = SwapUShortBE(Info->SecretLevel);
				Info->ParTime = SwapUShortBE(Info->ParTime);
				Info->ScenarioNum = SwapUShortBE(Info->ScenarioNum);
				Info->FloorNum = SwapUShortBE(Info->FloorNum);
			}
		}
		MapListPtr = Maps;
	}
	if (Songs != SongListPtr) {
		Length = ResourceLength(rSongList)>>1;
		for (i = 0; i < Length; i++)
			Songs[i] = SwapUShortBE(Songs[i]);
		SongListPtr = Songs;
	}
	if (Walls != WallListPtr) {
		Length = ResourceLength(MyWallList)>>1;
		for (i = 0; i < Length; i++)
			Walls[i] = SwapUShortBE(Walls[i]);
		WallListPtr = Walls;
	}
}

/**********************************

	Prepare the screen for game

**********************************/

void SetupPlayScreen (void)
{
	SetAPalette(rBlackPal);		/* Force black palette */
	ClearTheScreen(BLACK);		/* Clear the screen to black */
	BlastScreen();
	firstframe = 1;				/* fade in after drawing first frame */
	GameViewSize = NewGameWindow(GameViewSize);
	GrabMouse();
}

/**********************************

	Display the automap

**********************************/

exit_t RunAutoMap(void)
{
	Word vx,vy;
	Word Width,Height;
	Word CenterX,CenterY;
	Word oldjoy,newjoy;
	exit_t PS;

	MakeSmallFont();				/* Make the tiny font */
	playstate = EX_AUTOMAP;
	vx = viewx>>8;					/* Get my center x,y */
	vy = viewy>>8;
	Width = (SCREENWIDTH/16);		/* Width of map in tiles */
	Height = (VIEWHEIGHT/16);		/* Height of map in tiles */
	CenterX = Width/2;
	CenterY = Height/2;
	if (vx>=CenterX) {
		vx -= CenterX;
	} else {
		vx = 0;
	}
	if (vy>=CenterY) {
		vy -= CenterY;
	} else {
		vy = 0;
	}
	oldjoy = joystick1;
	for (;;) {
		WaitTick();
		ClearTheScreen(0x2f);
		DrawAutomap(vx,vy);
		for (;;) {
			PS = ReadSystemJoystick();
			if (PS || playstate != EX_AUTOMAP)
				goto Done;
			if (joystick1 != oldjoy || PauseExited)
				break;
			WaitTick();
		}
		oldjoy &= joystick1;
		newjoy = joystick1 ^ oldjoy;
		if (newjoy & (JOYPAD_START|JOYPAD_SELECT|JOYPAD_A|JOYPAD_B|JOYPAD_X|JOYPAD_Y)) {
			playstate = EX_STILLPLAYING;
		}
		if (newjoy & JOYPAD_UP && vy) {
			--vy;
		}
		if (newjoy & JOYPAD_LFT && vx) {
			--vx;
		}
		if (newjoy & JOYPAD_RGT && vx<(MAPSIZE-1)) {
			++vx;
		}
		if (newjoy & JOYPAD_DN && vy <(MAPSIZE-1)) {
			++vy;
		}
	}

Done:
	playstate = EX_STILLPLAYING;
/* let the player scroll around until the start button is pressed again */
	KillSmallFont();			/* Release the tiny font */
	RedrawStatusBar();
	if (!PS)
		PS = ReadSystemJoystick();
	mousex = 0;
	mousey = 0;
	mouseturn = 0;
	return PS;
}

/**********************************

	Begin a new game

**********************************/

void StartGame(void)
{
	if (playstate!=EX_LOADGAME) {	/* Variables already preset */
		NewGame();				/* init basic game stuff */
	}
	SetupPlayScreen();
	GameLoop();			/* Play the game */
	UngrabMouse();
	StopSong();			/* Make SURE music is off */
}

/**********************************

	Show the game logo

**********************************/

void TitleScreen (void)
{
	LongWord PackLen;
	LongWord *PackPtr;
	Byte *ShapePtr;
	exit_t PSTmp;
	exit_t PS = EX_COMPLETED;
	int Key;

	playstate = EX_LIMBO;	/* Game is not in progress */
	NewGameWindow(1);	/* Set to 512 mode */
	FadeToBlack();		/* Fade out the video */
	PackPtr = LoadAResource(rTitlePic);
	PackLen = SwapLongBE(PackPtr[0]);
	ShapePtr = (Byte *)AllocSomeMem(PackLen);
	DLZSS(ShapePtr,(Byte *) &PackPtr[1],PackLen);
	DrawShape(0,0,ShapePtr);
	ReleaseAResource(rTitlePic);
	BlastScreen();
	StartSong(SongListPtr[0]);
	FadeTo(rTitlePal);	/* Fade in the picture */
	BlastScreen();
	for (;;) {
		Key = WaitTicksEvent(0);		/* Wait for event */
		if (Key != '\e' && Key != -3)
			break;
		PSTmp = PauseMenu(FALSE);
		if (PSTmp > 0) {
			if (PSTmp == EX_RESTART)
				PSTmp = EX_LIMBO;
			PS = PSTmp;
			break;
		}
		DrawShape(0,0,ShapePtr);
		BlastScreen();
	}
	FreeSomeMem(ShapePtr);
	playstate = PS;
}

/**********************************

	Main entry point for the game (Called after InitTools)

**********************************/

extern Word NumberIndex;

int main()
{
	InitTools();		/* Init the system environment */
	WaitTick();			/* Wait for a system tick to go by */
	playstate = EX_LIMBO;
	UngrabMouse();
	NumberIndex = 36;	/* Force the score to redraw properly */
	IntermissionHack = FALSE;
	Intro();			/* Do the game intro */
	for (;;) {
		if (!playstate) {
			do {
				TitleScreen();		/* Show the game logo */
			} while (!playstate);
			StartSong(SongListPtr[0]);
			ClearTheScreen(BLACK);	/* Blank out the title page */
			BlastScreen();
			SetAPalette(rBlackPal);
		}
		if (playstate == EX_NEWGAME || ChooseGameDiff()) {	/* Choose your difficulty */
			playstate = EX_NEWGAME;	/* Start a new game */
			do {
				FadeToBlack();		/* Fade the screen */
				StartGame();		/* Play the game */
			} while (playstate == EX_LOADGAME || playstate == EX_NEWGAME);
		} else {
			playstate = EX_LIMBO;
		}
	}
}
