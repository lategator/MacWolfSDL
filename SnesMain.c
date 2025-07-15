#include "WolfDef.h"
#include <setjmp.h>
#include <SDL3/SDL_main.h>

/**********************************

	Load the map tables

**********************************/

void LoadMapSetData(void)
{
	LongWord Length;
	int i;

	SoundListPtr = (short int *) LoadAResource(MySoundList);	/* Get the list of sounds */
	Length = ResourceLength(MySoundList)>>1;
	for (i = 0; i < Length; i++)
		SoundListPtr[i] = SwapShortBE(SoundListPtr[i]);
	RegisterSounds(SoundListPtr);
	MapListPtr = (maplist_t *) LoadAResource(rMapList);	/* Get the map list */
	MapListPtr->MaxMap = SwapUShortBE(MapListPtr->MaxMap);
	MapListPtr->MapRezNum = SwapUShortBE(MapListPtr->MapRezNum);
	for (int i = 0; i < MapListPtr->MaxMap; i++) {
		MapInfo_t *Info = &MapListPtr->InfoArray[i];
		Info->NextLevel = SwapUShortBE(Info->NextLevel);
		Info->SecretLevel = SwapUShortBE(Info->SecretLevel);
		Info->ParTime = SwapUShortBE(Info->ParTime);
		Info->ScenarioNum = SwapUShortBE(Info->ScenarioNum);
		Info->FloorNum = SwapUShortBE(Info->FloorNum);
	}
	SongListPtr = (unsigned short *) LoadAResource(rSongList);
	Length = ResourceLength(rSongList)>>1;
	for (i = 0; i < Length; i++)
		SongListPtr[i] = SwapUShortBE(SongListPtr[i]);
	WallListPtr = (unsigned short *) LoadAResource(MyWallList);
	Length = ResourceLength(MyWallList)>>1;
	for (i = 0; i < Length; i++)
		WallListPtr[i] = SwapUShortBE(WallListPtr[i]);
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
}

/**********************************

	Display the automap

**********************************/

void RunAutoMap(void)
{
	Word vx,vy;
	Word Width,Height;
	Word CenterX,CenterY;
	Word oldjoy,newjoy;

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
	do {
		WaitTick();
		ClearTheScreen(0x2f);
		DrawAutomap(vx,vy);
		do {
			ReadSystemJoystick();
		} while (joystick1==oldjoy);
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
	} while (playstate==EX_AUTOMAP);

	playstate = EX_STILLPLAYING;
/* let the player scroll around until the start button is pressed again */
	KillSmallFont();			/* Release the tiny font */
	RedrawStatusBar();
	ReadSystemJoystick();
	mousex = 0;
	mousey = 0;
	mouseturn = 0;
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
	StopSong();			/* Make SURE music is off */
}

/**********************************

	Show the game logo

**********************************/

Boolean TitleScreen (void)
{
	LongWord PackLen;
	LongWord *PackPtr;
	Byte *ShapePtr;

	playstate = EX_LIMBO;	/* Game is not in progress */
	NewGameWindow(1);	/* Set to 512 mode */
	FadeToBlack();		/* Fade out the video */
	PackPtr = LoadAResource(rTitlePic);
	PackLen = SwapLongBE(PackPtr[0]);
	ShapePtr = (Byte *)AllocSomeMem(PackLen);
	DLZSS(ShapePtr,(Byte *) &PackPtr[1],PackLen);
	DrawShape(0,0,ShapePtr);
	ReleaseAResource(rTitlePic);
	FreeSomeMem(ShapePtr);
	BlastScreen();
	StartSong(SongListPtr[0]);
	FadeTo(rTitlePal);	/* Fade in the picture */
	BlastScreen();
	WaitTicksEvent(0);		/* Wait for event */
	playstate = EX_COMPLETED;
	return TRUE;				/* Return True if canceled */
}

/**********************************

	Main entry point for the game (Called after InitTools)

**********************************/

jmp_buf ResetJmp;
Boolean JumpOK;
extern Word NumberIndex;

int main()
{
	InitTools();		/* Init the system environment */
	WaitTick();			/* Wait for a system tick to go by */
	playstate = (exit_t)setjmp(ResetJmp);
	NumberIndex = 36;	/* Force the score to redraw properly */
	IntermissionHack = FALSE;
	if (playstate) {
		goto DoGame;	/* Begin a new game or saved game */
	}
	JumpOK = TRUE;		/* Jump vector is VALID */
	Intro();			/* Do the game intro */
	for (;;) {
		if (TitleScreen()) {		/* Show the game logo */
			StartSong(SongListPtr[0]);
			ClearTheScreen(BLACK);	/* Blank out the title page */
			BlastScreen();
			SetAPalette(rBlackPal);
			if (ChooseGameDiff()) {	/* Choose your difficulty */
				playstate = EX_NEWGAME;	/* Start a new game */
DoGame:
				FadeToBlack();		/* Fade the screen */
				StartGame();		/* Play the game */
			}
		}
	}
}
