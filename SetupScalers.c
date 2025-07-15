#include "WolfDef.h"
#include <string.h>

LongWord ScaleDiv[2048];			/* Divide table for scalers */

/**********************************

	Create the compiled scalers

**********************************/

Boolean SetupScalers(void)
{
	Word i;

	if (!ScaleDiv[1]) {		/* Divide table inited already? */
		i = 1;
		do {
			ScaleDiv[i] = 0x40000000/i;		/* Get the recipocal for the scaler */
		} while (++i<2048);
	}
	MaxScaler = 2048;		/* Init the highest scaler value */
	return TRUE;			/* No errors possible */
}

/**********************************

	Release the memory from the scalers

**********************************/

void ReleaseScalers(void)
{
}

/**********************************

	Draw a vertical line with a scaler
	(Used for WALL drawing)

**********************************/

void IO_ScaleWallColumn(Word X,Word Scale,Word Tile,Word Column)
{
	Byte *ScreenPtr, *ArtStart;
	LongWord Frac, Integer, Delta, i;
	Word Width;

	if (!Scale)
		return;
	Width = VideoWidth;
	ScreenPtr = VideoPointer + X;
	ArtStart = &ArtData[Tile][Column << 7];
	Frac = ScaleDiv[Scale];
	Integer = Frac >> 24;

	Scale <<= 1;
	if (Scale < VIEWHEIGHT) {
		i = Scale;
		Delta = 0;
		ScreenPtr += ((VIEWHEIGHT - Scale) >> 1) * Width;
	} else {
		i = VIEWHEIGHT;
		Delta = ((Scale - VIEWHEIGHT) >> 1) * Frac;
		ArtStart += Delta >> 24;
		Delta <<= 8;
	}
	Frac <<= 8;
	for (; i > 0; i--) {
		*ScreenPtr = *ArtStart;
		ArtStart += Integer + __builtin_add_overflow(Delta, Frac, &Delta);
		ScreenPtr += Width;
	}
}

/**********************************

	Draw a vertical line with a masked scaler
(Used for SPRITE drawing)

**********************************/

typedef struct {
	unsigned short Topy;
	unsigned short Boty;
	unsigned short Shape;
} SpriteRun;

void SpriteGlue(Byte *ArtStart,LongWord Frac,LongWord Integer,Byte *ScreenPtr,Word Count,LongWord Delta, Word Width)
{
	for (; Count > 0; Count--) {
		*ScreenPtr = *ArtStart;
		ArtStart += Integer + __builtin_add_overflow(Delta, Frac, &Delta);
		ScreenPtr += Width;
	}
}

void IO_ScaleMaskedColumn(Word x,Word scale,unsigned short *CharPtr,Word column)
{
	Byte * CharPtr2;
	int Y1,Y2;
	Byte *Screenad;
	const SpriteRun *RunPtr;
	LongWord TheFrac;
	LongWord TFrac;
	LongWord TInt;
	LongWord RunCount;
	int TopY;
	LongWord Index;
	LongWord Delta;
	Word Width;

	if (!scale) {
		return;
	}
	CharPtr2 = (Byte *) CharPtr;
	TheFrac = ScaleDiv[scale];		/* Get the scale fraction */
	RunPtr = (const SpriteRun *) &CharPtr[SwapUShortBE(CharPtr[column+1])/2];	/* Get the offset to the RunPtr data */
	Screenad = &VideoPointer[x];		/* Set the base screen address */
	TFrac = TheFrac<<8;
	TInt = TheFrac>>24;
	TopY = (int)(VIEWHEIGHT/2)-scale;		/* Number of pixels for 128 pixel shape */
	Width = VideoWidth;

	while (SwapUShortBE(RunPtr->Topy) != (unsigned short) -1) {		/* Not end of record? */
		Y1 = scale*(LongWord)SwapUShortBE(RunPtr->Topy)/128+TopY;
		if (Y1<(int)VIEWHEIGHT) {		/* Clip top? */
			Y2 = scale*(LongWord)SwapUShortBE(RunPtr->Boty)/128+TopY;
			if (Y2>0) {

				if (Y2>(int)VIEWHEIGHT) {
					Y2 = VIEWHEIGHT;
				}
				Index = SwapUShortBE(RunPtr->Shape)+(SwapUShortBE(RunPtr->Topy)/2);
				Delta = 0;
				if (Y1<0) {
					Delta = -Y1*TheFrac;
					Index += (Delta>>24);
					Delta <<= 8;
					Y1 = 0;
				}
				RunCount = Y2-Y1;
				if (RunCount) {
					SpriteGlue(
						&CharPtr2[Index],	/* Pointer to art data */
						TFrac,				/* Fractional value */
						TInt,				/* Integer value */
						&Screenad[Y1*Width],			/* Pointer to screen */
						RunCount,			/* Number of lines to draw */
						Delta,					/* Delta value */
						Width
					);
				}
			}
		}
		RunPtr++;						/* Next record */
	}
}

/**********************************

	Draw an automap tile

**********************************/

Byte *SmallFontPtr;

void DrawSmall(Word x,Word y,Word tile)
{
	Byte *Screenad;
	Byte *ArtStart;
	Word Width,Height;

	if (!SmallFontPtr) {
		return;
	}
	x*=16;
	y*=16;
	Screenad = &VideoPointer[YTable[y]+x];
	ArtStart = &SmallFontPtr[tile*(16*16)];
	Height = 0;
	do {
		Width = 16;
		do {
			Screenad[0] = ArtStart[0];
			++Screenad;
			++ArtStart;
		} while (--Width);
		Screenad+=VideoWidth-16;
	} while (++Height<16);
}

void MakeSmallFont(void)
{
	Word i,j,Width,Height;
	Byte *DestPtr,*ArtStart;
	Byte *TempPtr;

	SmallFontPtr = AllocSomeMem(16*16*69);
	if (!SmallFontPtr) {
		return;
	}
	memset(SmallFontPtr,0,16*16*69);		/* Erase the font */
	i = 0;
	DestPtr = SmallFontPtr;
	do {
		ArtStart = &ArtData[i][0];
		if (!ArtStart) {
			DestPtr+=(16*16);
		} else {
			Height = 0;
			do {
				Width = 16;
				j = Height*8;
				do {
					DestPtr[0] = ArtStart[j];
					++DestPtr;
					j+=(WALLHEIGHT*8);
				} while (--Width);
			} while (++Height<16);
		}
	} while (++i<64);
	if (ResourceLength(MyBJFace) >= 16*16) {
		TempPtr = LoadAResource(MyBJFace);
		if (ResourceLength(MyBJFace) >= 16*16*5) {
			memcpy(DestPtr,TempPtr,16*16*5);
		} else {
			for (i = 0; i < 5; i++)
				DestPtr = memcpy(DestPtr,TempPtr,16*16) + 16*16;
		}
		ReleaseAResource(MyBJFace);
	}

#if 0
	TempPtr = AllocSomeMem(16*16*128);
	memset(TempPtr,-1,16*16*128);
	i = 0;
	do {
		memcpy(&TempPtr[(i+1)*256],&SmallFontPtr[(i*2)*256],256);
	} while (++i<29);
	i = 0;
	do {
		memcpy(&TempPtr[((i*2)+90)*256],&SmallFontPtr[(i+59)*256],256);
		memcpy(&TempPtr[((i*2)+91)*256],&SmallFontPtr[(i+59)*256],256);
	} while (++i<4);
	SaveJunk(TempPtr,256*128);
	FreeSomeMem(TempPtr);
#endif
}

void KillSmallFont(void)
{
	if (SmallFontPtr) {
		FreeSomeMem(SmallFontPtr);
		SmallFontPtr=0;
	}
}
