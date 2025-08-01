#include "WolfDef.h"
#include <string.h>
#include <stdlib.h>

extern Word NumberIndex;	/* Hack for drawing numbers */
static LongWord BJTime;	/* Time to draw BJ? */
static Word WhichBJ;	/* Which BJ to show */
static Word *BJPtrs[3];		/* Pointers to BJ's true shapes */
static LongWord *BJIndexPtr;			/* Pointer to indexes of BJ's shapes */
static Word ParTime;		/* Par time for level */
static LongWord BonusScore;	/* Additional points */

#define BONUSX	353
#define BONUSY	103
#define	TIMEX	353
#define TIMEWIDTH 36
#define	TIMEY	140
#define	TIMEY2	180
#define SCOREX 353
#define SCOREY 332
#define	RATIOX	353
#define	RATIOY	217
#define	RATIOY2	253
#define	RATIOY3	291

/**********************************

	Draw BJ if needed

**********************************/

static Rect BJRect = {48,73,48+142,73+131};	/* Rect for BJ's picture */
static void ShowBJ(void)
{
	if (!BJIndexPtr)
		return;
	if ((ReadTick()-BJTime) >= 20) {		/* Time to draw a BJ? */
		BJTime = ReadTick();				/* Set the new time */
		if (WhichBJ!=2) {			/* Thumbs up? */
			WhichBJ ^= 1;			/* Nope, toggle breathing */
		}
		if (!BJPtrs[WhichBJ])
			return;
		DrawShape(73,48,BJPtrs[WhichBJ]);		/* Draw BJ */
		BlastScreen2(&BJRect);				/* Update video */
	}
}

/**********************************

	Have BJ Breath for a while

**********************************/

static void BJBreath(Word Delay)
{
	do {
		ShowBJ();
		if (WaitTicksEvent(1)) {
			break;
		}
	} while (--Delay);
}

/**********************************

	Draw the score

**********************************/

static Rect ScoreRect = {SCOREY,SCOREX,SCOREY+22,SCOREX+(12*7)};

static void DrawIScore(void)
{
	SetNumber(gamestate.score,SCOREX,SCOREY,7);		/* Draw the game score */
	BlastScreen2(&ScoreRect);
}

/**********************************

	Draw the earned bonus

**********************************/

static Rect BonusRect = {BONUSY,BONUSX,BONUSY+22,BONUSX+(12*7)};
static void DrawIBonus(void)
{
	SetNumber(BonusScore,BONUSX,BONUSY,7);
	BlastScreen2(&BonusRect);
}

/**********************************

	Draw a time value at the given coords

**********************************/

static void DrawTime(Word x,Word y,Word time)
{
	Word minutes,seconds;
	Rect TimeRect;

	TimeRect.left = x;
	TimeRect.right = x+((12*4)+TIMEWIDTH);
	TimeRect.top = y;
	TimeRect.bottom = y+22;

	minutes = time/60;
	seconds = time%60;
	SetNumber(minutes,x,y,2);
	x+=TIMEWIDTH;
	SetNumber(seconds,x,y,2);
	BlastScreen2(&TimeRect);
}

/**********************************

	Draws a ratio value at the given coords.

**********************************/

static void DrawRatio(Word x,Word y,Word theRatio)
{
	Rect RatioRect;

	RatioRect.top = y;
	RatioRect.left = x;
	RatioRect.bottom = y+22;
	RatioRect.right = x+(3*12);
	SetNumber(theRatio,x,y,3);
	BlastScreen2(&RatioRect);
}

/**********************************

	RollScore
	Do a Bill-Budgey roll of the old score to the new score,
	not bothering with the lower digit, as you never get less
	than ten for anything.

**********************************/

static void RollScore(void)
{
	Word i;

	do {
		if (BonusScore>1000) {
			i = 1000;
		} else {
			i = BonusScore;
		}
		BonusScore-=i;
		GivePoints(i);
		ShowBJ();
		DrawIScore();
		DrawIBonus();
		PlaySound(SND_MGUN|0x8000);
		if (WaitTicksEvent(6)) {
			GivePoints(BonusScore);	/* Add the final bonus */
			BonusScore=0;
			DrawIScore();
			DrawIBonus();
			break;
		}
	} while (BonusScore);
}

/**********************************

	RollRatio
	Do a Bill-Budgey roll of the ratio.

**********************************/

static void RollRatio(Word x,Word y,Word ratio)
{
	Word i;
	Word NoDelay;

	i = 0;
	NoDelay = 0;
	while (i<ratio) {
		DrawRatio(x,y,i);
		PlaySound(SND_MGUN|0x8000);
		ShowBJ();
		if (WaitTicksEvent(6)) {
			NoDelay = 1;
			break;
		}
		i+=10;
	}
	DrawRatio(x,y,ratio);

	/* make ding sound */

	if (ratio==100) {
		if (!NoDelay) {
			PlaySound(SND_EXTRA);
			WaitTicks(30);
		}
		BonusScore += 10000;
		DrawIBonus();
		if (!NoDelay) {
			BJBreath(60);	/* Breath a little */
		}
	}
}

/**********************************

	Let's show 'em how they did!

**********************************/

void LevelCompleted (void)
{
	Word i, k;
	Word *ShapePtr;
	LongWord UnpackLength;
	LongWord Index;

/* setup */

	ParTime = MapListPtr->InfoArray[gamestate.mapon].ParTime;
	BonusScore = 0;		/* Init the bonus */

	IntermissionHack = TRUE;	/* Hack to keep score from drawing twice */
	NumberIndex = 47;		/* Hack to draw score using an alternate number set */
	UngrabMouse();
	NewGameWindow(1);		/* Force 512 mode screen */
	ShapePtr = LoadCompressedShape(rIntermission);
	if (ShapePtr) {
		DrawShape(0,0,ShapePtr);
		FreeSomeMem(ShapePtr);
	}
	BJIndexPtr = LoadCompressed(rInterPics, &UnpackLength);
	if (BJIndexPtr) {
		if (UnpackLength < sizeof(LongWord)*3) {
			FreeSomeMem(BJIndexPtr);
			BJIndexPtr = NULL;
		} else {
			for (i = 0; i < 3; i++) {
				BJPtrs[i] = NULL;
				Index = SwapLongBE(BJIndexPtr[i]);
				if (UnpackLength >= Index + 4) {
					ShapePtr = (void*)(((Byte*)BJIndexPtr) + Index);
					if (UnpackLength >= Index + 4 + SwapUShortBE(ShapePtr[0]) * SwapUShortBE(ShapePtr[1]))
						BJPtrs[i] = ShapePtr;
				}
			}
		}
	}

	WhichBJ = 0;		/* Init BJ */
	BJTime = ReadTick()-50;		/* Force a redraw */
	BlastScreen();		/* Draw the screen */
	ShowBJ();			/* Draw BJ */
	StartSong(1);	/* Play the intermission song */
	SetAPalette(rInterPal);	/* Set the palette */
	DrawIScore();			/* Draw the current score */

	/* First an initial pause */

	BJBreath(60);

	/* Display Par Time, Player's Time, and show bonus if any. */

	if (gamestate.playtime>=(100*60*60UL)) {
		k =(99*60)+59;
	} else {
		k = gamestate.playtime/60;
	}
	DrawTime(TIMEX,TIMEY,k);		/* How much time has elapsed? */
	DrawTime(TIMEX,TIMEY2,ParTime);

	if (k < ParTime) {
		k = (ParTime-k) * 50;		/* 50 points per second */
		BonusScore += k;		/* Add to the bonus */
		DrawIBonus();			/* Draw the bonus */
		PlaySound(SND_EXTRA);
		BJBreath(60);			/* Breath a little */
	}

/* Show ratios for "terminations", treasure, and secret stuff. */
/* If 100% on all counts, Perfect Bonus! */

	k=0;		/* Not perfect (Yet) */
	RollRatio(RATIOX,RATIOY,(gamestate.treasurecount*100)/gamestate.treasuretotal);
	if (gamestate.treasurecount == gamestate.treasuretotal) {
		k++;			/* Perfect treasure */
	}
	RollRatio(RATIOX,RATIOY2,(gamestate.killcount*100)/gamestate.killtotal);
	if (gamestate.killcount == gamestate.killtotal) {
		k++;			/* Perfect kills */
	}
	RollRatio(RATIOX,RATIOY3,(gamestate.secretcount*100)/gamestate.secrettotal);
	if (gamestate.secretcount == gamestate.secrettotal) {
		k++;			/* Perfect secret */
	}
	if (BonusScore) {	/* Did you get a bonus? */
		RollScore();
		BJBreath(60);
	}
	if (k==3) {
		WhichBJ = 2;	/* Draw thumbs up for BJ */
		PlaySound(SND_THUMBSUP);
	}
	do {
		ShowBJ();		/* Animate BJ */
	} while (!WaitTicksEvent(1));		/* Wait for a keypress */
	if (BJIndexPtr)
		FreeSomeMem(BJIndexPtr);		/* Release BJ's shapes */
	FadeToBlack();		/* Fade away */
	IntermissionHack = FALSE;		/* Release the hack */
	NumberIndex = 36;			/* Restore the index */
}

/**********************************

	Handle the intermission screen

**********************************/

void Intermission (void)
{
	FadeToBlack();
	LevelCompleted();			/* Show the data (Init ParTime) */
	gamestate.globaltime += gamestate.playtime;		/* Add in the playtime */
	gamestate.globaltimetotal += ParTime;	/* Get the par */
	gamestate.globalsecret += gamestate.secretcount;	/* Secrets found */
	gamestate.globaltreasure += gamestate.treasurecount;	/* Treasures found */
	gamestate.globalkill += gamestate.killcount;	/* Number killed */
	gamestate.globalsecrettotal += gamestate.secrettotal;	/* Total secrets */
	gamestate.globaltreasuretotal += gamestate.treasuretotal;	/* Total treasures */
	gamestate.globalkilltotal += gamestate.killtotal;	/* Total kills */
	SetupPlayScreen();		/* Reset the game screen */
}

/**********************************

	Okay, let's face it: they won the game.

**********************************/

void VictoryIntermission (void)
{
	FadeToBlack();
	LevelCompleted();
}

/**********************************

	Show player the game characters

**********************************/

#define NUMCAST 12
Word caststate[NUMCAST] = {
ST_GRD_WLK1, ST_OFC_WLK1, ST_SS_WLK1,ST_MUTANT_WLK1,
ST_DOG_WLK1, ST_HANS_WLK1, ST_SCHABBS_WLK1, ST_TRANS_WLK1,
ST_UBER_WLK1, ST_DKNIGHT_WLK1,ST_MHITLER_WLK1, ST_HITLER_WLK1};

#if 0
char *casttext[NUMCAST] = { /* 28 chars max */
"GUARD",
"OFFICER",
"ELITE GUARD",
"MUTANT",
"MUTANT RAT",
"HANS GROSSE",
"DR. SCHABBS",
"TRANS GROSSE",
"UBERMUTANT",
"DEATH KNIGHT",
"MECHAMEISTER",
"STAATMEISTER"
};
#endif

exit_t CharacterCast(void)
{
	Word Enemy,count, cycle;
	Word up;
	exit_t PS = 0;
	state_t *StatePtr;

/* reload level and set things up */

	gamestate.mapon = 0;		/* First level again */
	PrepPlayLoop();				/* Prepare the system */
	viewx = actors[0].x;		/* Mark the starting x,y */
	viewy = actors[0].y;

	topspritescale = 32*2;

/* go through the cast */

	Enemy = 0;
	cycle = 0;
	do {
		StatePtr = &states[caststate[Enemy]];		/* Init the state pointer */
		count = 1;			/* Force a fall through on first pass */
		up = FALSE;
		for (;;) {
			if (++cycle>=60*4) {		/* Time up? */
				cycle = 0;				/* Reset the clock */
				if (++Enemy>=NUMCAST) {	/* Next bad guy */
					Enemy = 0;			/* Reset the bad guy */
				}
				break;
			}
			if (!--count) {
				count = StatePtr->tictime;
				StatePtr = &states[StatePtr->next];
			}
			topspritenum = StatePtr->shapenum;	/* Set the formost shape # */
			RenderView();		/* Show the 3d view */
			WaitTicks(1);		/* Limit to 15 frames a second */
			PS = ReadSystemJoystick();	/* Read the joystick */
			if (PS)
				return PS;
			if (!joystick1 && !up) {
				up = TRUE;
				continue;
			}
			if (!up) {
				continue;
			}
			if (!joystick1) {
				continue;
			}
			if (joystick1 & (JOYPAD_START|JOYPAD_A|JOYPAD_B|JOYPAD_X|JOYPAD_Y)) {
				Enemy = NUMCAST;
				break;
			}
			if ( (joystick1 & (JOYPAD_TL|JOYPAD_LFT)) && Enemy >0) {
				Enemy--;
				break;
			}
			if ( (joystick1 & (JOYPAD_TR|JOYPAD_RGT)) && Enemy <NUMCAST-1) {
				Enemy++;
				break;
			}
		}
	} while (Enemy < NUMCAST);	/* Still able to show */
	StopSong();		/* Stop the music */
	FadeToBlack();	/* Fade out */
	return PS;
}
