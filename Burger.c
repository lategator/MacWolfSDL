/**********************************

	Burger library for the Macintosh.
	Use Think.c or Code Warrior to compile.
	Use SMART linking to link in just what you need

**********************************/

#include "Burger.h"
#include "WolfDef.h"		/* Get the prototypes */
#include <string.h>
#include <err.h>
#include <SDL3/SDL.h>

/**********************************

	Variables used by my global library

**********************************/

Word DoEvent(SDL_Event *event);
void BlastScreen(void);

Word NoSystemMem;
unsigned char *VideoPointer;	/* Pointer to video memory */
extern Word QuitFlag;			/* Did the application quit? */
Word VideoWidth;				/* Width to each video scan line */
Word SystemState=3;				/* Sound on/off flags */
Word KilledSong;				/* Song that's currently playing */
LongWord LastTick;				/* Last system tick (60hz) */
Word FontX;						/* X Coord of font */
Word FontY;						/* Y Coord of font */
unsigned char *FontPtr;			/* Pointer to font image data */
unsigned char *FontWidths;		/* Pointer to font width table */
Word FontHeight;				/* Point size of current font */
Word FontLast;					/* Number of font entries */
Word FontFirst;					/* ASCII value of first char */
Word FontLoaded;				/* Rez number of loaded font (0 if none) */
Word FontInvisible;				/* Allow masking? */
unsigned char FontOrMask[16];	/* Colors for font */
LongWord YTable[480];			/* Offsets to the screen */
//SndChannelPtr myPaddleSndChan;	/* Sound channel */
//Word ScanCode;
//CWindowPtr GameWindow;
//CGrafPtr GameGWorld;
//extern GDHandle gMainGDH;
//extern CTabHandle MainColorHandle;
//extern Boolean DoQuickDraw;

/**********************************

	Wait a single system tick

**********************************/


/**********************************

	Wait a single system tick

**********************************/

#define TICK_NS (1000000000ULL / 60ULL)

void WaitTick(void)
{
	Uint64 Delay, Ticks;

	Ticks = SDL_GetTicksNS();
	Delay = (LastTick + 1) * TICK_NS - Ticks;
	if (((Sint64) Delay) > 0) {
		SDL_DelayNS(Delay);
		LastTick = Ticks / TICK_NS + 1;
	} else {
		LastTick = Ticks / TICK_NS;
	}
}

/**********************************

	Wait a specific number of system ticks
	from a time mark before you get control

**********************************/

#define TICK_NS (1000000000ULL / 60ULL)

void WaitTicks(Word Count)
{
	SDL_Event event;

	for (; Count; Count--) {
		WaitTick();
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT)
				GoodBye();
		}
	}
}

/**********************************

	Get the current system tick

**********************************/

LongWord ReadTick(void)
{
	return(SDL_GetTicksNS() * 60 / 1000000000ULL);	/* Just get it from the Mac OS */
}

/**********************************

	Wait for a mouse/keyboard event

**********************************/

Word WaitEvent(void)
{
	Word Temp;
	do {
		Temp = WaitTicksEvent(6000);	/* Wait 10 minutes */
	} while (!Temp);	/* No event? */
	return Temp;		/* Return the event code */
}

/**********************************

	Wait for an event or a timeout

**********************************/

Word WaitTicksEvent(Word Time)
{
	SDL_Event event;

	for (Word i = Time;; i--) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT)
				GoodBye();
			else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
				return 0;
			else if (event.type == SDL_EVENT_KEY_DOWN)
				return event.key.key;
		}
		if (Time && !i)
			break;
		WaitTick();
	}
	return 0;
}

/**********************************

	Convert a long value into a ascii string

**********************************/

static LongWord Tens[] = {
	1,
	10,
	100,
	1000,
	10000,
	100000,
	1000000,
	10000000,
	100000000,
	1000000000};

void ultoa(LongWord Val,char *Text)
{
	Word Index;		/* Index to Tens table */
	Word Hit;		/* Printing? */
	Word Letter;	/* Letter to print */
	LongWord LongVal;	/* Tens value */

	Index = 9;		/* Start at the millions */
	Hit = 0;		/* Didn't print anything yet! */
	do {
		Letter = '0';	/* Init the char */
		LongVal = Tens[Index];	/* Init the value into a local */
		while (Val >= LongVal) {	/* Is the number in question larger? */
			Val -= LongVal;		/* Sub the tens value */
			++Letter;			/* Inc the char */
			Hit=1;				/* I must draw! */
		}
		if (Hit) {	/* Remove the leading zeros */
			Text[0] = Letter;	/* Save char in the string */
			++Text;			/* Inc dest */
		}
	} while (--Index);		/* All the tens done? */
	Text[0] = Val + '0';	/* Must print FINAL digit */
	Text[1] = 0;			/* End the string */
}

/**********************************

	Sound sub-system

**********************************/

/**********************************

	Shut down the sound

**********************************/

void SoundOff(void)
{
	PlaySound(0);
}

/**********************************

	Play a sound resource

**********************************/

void PlaySound(Word SoundNum)
{
	if (SoundNum && (SystemState&SfxActive)) {
		SoundNum+=127;
		if (SoundNum&0x8000) {		/* Mono sound */
			EndSound(SoundNum&0x7fff);
		}
		BeginSound(SoundNum&0x7fff);
	} else {
		EndAllSound();
	}
}

/**********************************

	Stop playing a sound resource

**********************************/

void StopSound(Word SoundNum)
{
	EndSound(SoundNum+127);
}

static Word LastSong = -1;

void PlaySong(Word Song)
{
	if (Song) {
		KilledSong = Song;
		if (SystemState&MusicActive) {
			if (Song!=LastSong) {
				//BeginSongLooped(Song);
				LastSong = Song;
			}
			return;
		}
	}
	//EndSong();
	LastSong = -1;
}

/**********************************

	Graphics subsystem

**********************************/

/**********************************

	Draw a masked shape

**********************************/

void InitYTable(void)
{
	Word i;
	LongWord Offset;

	i = 0;
	Offset = 0;
	do {
		YTable[i] = Offset;
		Offset+=VideoWidth;
	} while (++i<480);
}

/**********************************

	Draw a shape

**********************************/

void DrawShape(Word x,Word y,void *ShapePtr)
{
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *ShapePtr2;
	unsigned short *ShapePtr3;
	Word Width;
	Word Height;
	Word Width2;

	ShapePtr3 = ShapePtr;
	Width = SwapUShortBE(ShapePtr3[0]);		/* 16 bit width */
	Height = SwapUShortBE(ShapePtr3[1]);		/* 16 bit height */
	ShapePtr2 = (unsigned char *) &ShapePtr3[2];
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[y]+x];
	do {
		Width2 = Width;
		Screenad = ScreenPtr;
		do {
			*Screenad++ = *ShapePtr2++;
		} while (--Width2);
		ScreenPtr +=VideoWidth;
	} while (--Height);
}

/**********************************

	Draw a masked shape

**********************************/

void DrawMShape(Word x,Word y,void *ShapePtr)
{
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *MaskPtr;
	unsigned char *ShapePtr2;
	Word Width;
	Word Height;
	Word Width2;

	ShapePtr2 = ShapePtr;
	Width = ShapePtr2[1];
	Height = ShapePtr2[3];
	ShapePtr2 +=4;
	MaskPtr = &ShapePtr2[Width*Height];
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[y]+x];
	do {
		Width2 = Width;
		Screenad = ScreenPtr;
		do {
			*Screenad = (*Screenad & *MaskPtr++) | *ShapePtr2++;
			++Screenad;
		} while (--Width2);
		ScreenPtr +=VideoWidth;
	} while (--Height);
}

/**********************************

	Draw a masked shape with an offset

**********************************/

void DrawXMShape(Word x,Word y,void *ShapePtr)
{
	unsigned short *ShapePtr2;
	ShapePtr2 = ShapePtr;
	x += SwapUShortBE(ShapePtr2[0]);
	y += SwapUShortBE(ShapePtr2[1]);
	DrawMShape(x,y,&ShapePtr2[2]);
}

/**********************************

	Erase a masked shape

**********************************/

void EraseMBShape(Word x,Word y, void *ShapePtr, void *BackPtr)
{
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *Backad;
	unsigned char *BackPtr2;
	unsigned char *MaskPtr;
	Word Width;
	Word Height;
	Word Width2;

	MaskPtr = ShapePtr;		/* Get the pointer to the mask */
	Width = MaskPtr[1];		/* Get the width of the shape */
	Height = MaskPtr[3];	/* Get the height of the shape */
	MaskPtr = &MaskPtr[(Width*Height)+4];	/* Index to the mask */
							/* Point to the screen */
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[y]+x];
	BackPtr2 = BackPtr;
	BackPtr2 = &BackPtr2[(y*SCREENWIDTH)+x];	/* Index to the erase buffer */
	do {
		Width2 = Width;		/* Init width count */
		Screenad = ScreenPtr;
		Backad = BackPtr2;
		do {
			if (!*MaskPtr++) {
				*Screenad = *Backad;
			}
			++Screenad;
			++Backad;
		} while (--Width2);
		ScreenPtr +=VideoWidth;
		BackPtr2 += SCREENWIDTH;
	} while (--Height);
}

/**********************************

	Test for a shape collision

**********************************/

Word TestMShape(Word x,Word y,void *ShapePtr)
{
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *MaskPtr;
	unsigned char *ShapePtr2;
	Word Width;
	Word Height;
	Word Width2;

	ShapePtr2 = ShapePtr;
	Width = ShapePtr2[0];
	Height = ShapePtr2[1];
	ShapePtr2 +=2;
	MaskPtr = &ShapePtr2[Width*Height];
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[y]+x];
	do {
		Width2 = Width;
		Screenad = ScreenPtr;
		do {
			if (!*MaskPtr++) {
				if (*Screenad != *ShapePtr2) {
					return 1;
				}
			}
			++ShapePtr2;
			++Screenad;
		} while (--Width2);
		ScreenPtr +=VideoWidth;
	} while (--Height);
	return 0;
}

/**********************************

	Test for a masked shape collision

**********************************/

Word TestMBShape(Word x,Word y,void *ShapePtr,void *BackPtr)
{
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *Backad;
	unsigned char *BackPtr2;
	unsigned char *MaskPtr;
	Word Width;
	Word Height;
	Word Width2;

	MaskPtr = ShapePtr;		/* Get the pointer to the mask */
	Width = MaskPtr[0];		/* Get the width of the shape */
	Height = MaskPtr[1];	/* Get the height of the shape */
	MaskPtr = &MaskPtr[(Width*Height)+2];	/* Index to the mask */
							/* Point to the screen */
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[y]+x];
	BackPtr2 = BackPtr;
	BackPtr2 = &BackPtr2[(y*SCREENWIDTH)+x];	/* Index to the erase buffer */
	do {
		Width2 = Width;		/* Init width count */
		Screenad = ScreenPtr;
		Backad = BackPtr2;
		do {
			if (!*MaskPtr++) {
				if (*Screenad != *Backad) {
					return 1;
				}
			}
			++Screenad;
			++Backad;
		} while (--Width2);
		ScreenPtr +=VideoWidth;
		BackPtr2 += SCREENWIDTH;
	} while (--Height);
	return 0;
}

/**********************************

	Show a full screen picture

**********************************/

void ShowPic(Word PicNum)
{
	DrawShape(0,0,LoadAResource(PicNum));	/* Load the resource and show it */
	ReleaseAResource(PicNum);			/* Release it */
	BlastScreen();
}

/**********************************

	Clear the screen to a specific color

**********************************/

void ClearTheScreen(Word Color)
{
	Word x,y;
	unsigned char *TempPtr;

	TempPtr = VideoPointer;
	y = SCREENHEIGHT;		/* 200 lines high */
	do {
		x = 0;
		do {
			TempPtr[x] = Color;	/* Fill color */
		} while (++x<SCREENWIDTH);
		TempPtr += VideoWidth;	/* Next line down */
	} while (--y);
}

/**********************************

	Palette Manager

**********************************/

/**********************************

	Load and set a palette resource

**********************************/

void SetAPalette(Word PalNum)
{
	SetAPalettePtr(LoadAResource(PalNum));		/* Set the current palette */
	ReleaseAResource(PalNum);					/* Release the resource */
}

/**********************************

	Fade the screen to black

**********************************/

void FadeToBlack(void)
{
	unsigned char MyPal[768];

	memset(MyPal,0,sizeof(MyPal));	/* Fill with black */
	MyPal[0] = MyPal[1] = MyPal[2] = 255;
	FadeToPtr(MyPal);
}

/**********************************

	Fade the screen to a palette

**********************************/

void FadeTo(Word RezNum)
{
	FadeToPtr(LoadAResource(RezNum));
	ReleaseAResource(RezNum);
}

/**********************************

	Fade the palette

**********************************/

extern Byte CurrentPal[768];

void FadeToPtr(unsigned char *PalPtr)
{
	int DestPalette[768];				/* Dest offsets */
	Byte WorkPalette[768];		/* Palette to draw */
	Byte SrcPal[768];
	Word Count;
	Word i;

	if (!memcmp(PalPtr,&CurrentPal,768)) {	/* Same palette? */
		return;
	}
	memcpy(SrcPal,CurrentPal,768);
	i = 0;
	do {		/* Convert the source palette to ints */
		DestPalette[i] = PalPtr[i];
	} while (++i<768);

	i = 0;
	do {
		DestPalette[i] -= SrcPal[i];	/* Convert to delta's */
	} while (++i<768);

	Count = 1;
	do {
		i = 0;
		do {
			WorkPalette[i] = ((DestPalette[i] * (int)(Count)) / 16) + SrcPal[i];
		} while (++i<768);
		SetAPalettePtr(WorkPalette);
		BlastScreen();
		WaitTicks(1);
	} while (++Count<17);
}

void KillAResource(Word RezNum)
{
	ReleaseAResource(RezNum);
}

/*
void SaveJunk(void *AckPtr,Word Length)
{
static Word Count=1;
	FILE *fp;
	char SaveName[40];
	sprintf(SaveName,"SprRec%d",Count);
	fp = fopen(SaveName,"wb");
	fwrite(AckPtr,1,Length,fp);
	fclose(fp);
	++Count;
}
*/

/**********************************

	Allocate some memory

**********************************/

void *AllocSomeMem(LongWord Size)
{
	return SDL_malloc(Size);
}

/**********************************

	Release some memory

**********************************/

void FreeSomeMem(void *MemPtr)
{
	SDL_free(MemPtr);
}
