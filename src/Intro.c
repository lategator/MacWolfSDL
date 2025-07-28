#include "WolfDef.h"
#include <ctype.h>
#include <stdlib.h>

/**********************************

	Main game introduction

**********************************/

void Intro(void)
{
	Byte *ShapePtr;

	NewGameWindow(1);	/* Set to 512 mode */

	FadeToBlack();		/* Fade out the video */
	ShapePtr = LoadCompressedShape(rMacPlayPic);
	if (ShapePtr) {
		DrawShape(0,0,ShapePtr);
		FreeSomeMem(ShapePtr);
		BlastScreen();
	}

	StartSong(0);	/* Play the song */
	FadeTo(rMacPlayPal);	/* Fade in the picture */
	WaitTicksEvent(240);		/* Wait for event */
	FadeTo(rIdLogoPal);
	if (toupper(WaitTicksEvent(240))=='B') {		/* Wait for event */
		FadeToBlack();
		ClearTheScreen(BLACK);
		BlastScreen();
		ShapePtr = LoadCompressedShape(rYummyPic);
		if (ShapePtr) {
			DrawShape((SCREENWIDTH-320)/2,(SCREENHEIGHT-200)/2,ShapePtr);
			FreeSomeMem(ShapePtr);
			BlastScreen();
		}
		FadeTo(rYummyPal);
		WaitTicksEvent(600);
	}
}
