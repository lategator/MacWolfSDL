#include "WolfDef.h"
#include <string.h>

/**********************************

	Sorts the events from xevents[0] to xevent_p
	firstevent will be set to the first sorted event

**********************************/

void SimpleSort(void)
{
	Boolean Moved;
	Word i,j;
	vissprite_t *Temp;

	if (numvisspr <= 1)
		return;

	i = numvisspr - 1;
	do {
		Moved = FALSE;
		j = 0;
		do {
			if (xevents[j + 1]->clipscale < xevents[j]->clipscale) {
				Temp = xevents[j];
				xevents[j] = xevents[j + 1];
				xevents[j + 1] = Temp;
				Moved = TRUE;
			}
			j = j + 1;
		} while (j < i);
	} while (Moved);
}

/**********************************

	Draw a single scaled sprite
	x1 = Left edge, x2 = Right edge, rs_vseg = record for sprite

**********************************/

void RenderSprite(vissprite_t *VisPtr)
{
	Word column;
	Word scaler;
	short x1,x2;
	short mid;

	x1 = VisPtr->x1;
	if (x1<0) {		/* Clip the left? */
		x1 = 0;
	}
	x2 = VisPtr->x2;
	if (x2>= (int)SCREENWIDTH) {	/* Clip the right? */
		x2 = SCREENWIDTH-1;
	}
	scaler = VisPtr->clipscale;		/* Get the size of the sprite */
	column = 0;					/* Start at the first column */
	if ((int) x1 > VisPtr->x1) {		/* Clip the left? */
		column = (x1-VisPtr->x1)*VisPtr->columnstep;	/* Start here instead */
	}

/* calculate and draw each column */

	if (VisPtr->actornum == (Word)-1) {
		mid = VisPtr->x1 + ((VisPtr->x2-VisPtr->x1)>>1);
		if (mid >= 0 && mid < SCREENWIDTH && xscale[mid] <= scaler) {
			do {
				IO_ScaleMaskedColumn(x1,scaler,VisPtr->pos,column>>FRACBITS);
				column+=VisPtr->columnstep;		/* Next column (Fraction) */
			} while (++x1<=x2);
		}
	} else {
		do {
			if (xscale[x1] <= scaler) {	/* Visible? */
				IO_ScaleMaskedColumn(x1,scaler,VisPtr->pos,column>>FRACBITS);
			}
			column+=VisPtr->columnstep;		/* Next column (Fraction) */
		} while (++x1<=x2);
	}
}

/**********************************

	Add a sprite entry to the render list

**********************************/

void AddSprite (thing_t *thing,Word actornum)
{
	fixed_t tx;			/* New X coord */
	fixed_t tz;			/* New z coord (Size) */
	Word scale;			/* Scaled size */
	int	px;				/* Center X coord */
	unsigned short *patch;	/* Pointer to sprite data */
	int	x1, x2;			/* Left,Right x */
	Word width;			/* Width of sprite */
	fixed_t trx,try;	/* x,y from the camera */
	vissprite_t *VisPtr;	/* Local pointer to visible sprite record */

/* transform the origin point */

	if (numvisspr>=(MAXVISSPRITES-1)) {
		return;
	}
	trx = thing->x - viewx;		/* Adjust from the camera view */
	try = viewy - thing->y;		/* Adjust from the camera view */
	tz = R_TransformZ(trx,try);	/* Get the distance */

	if (tz < MINZ) {		/* Too close? */
		return;
	}

	if (tz>=MAXZ) {		/* Force smallest */
		tz = MAXZ-1;
	}
	scale = scaleatzptr[tz];	/* Get the scale at the z coord */
	tx = R_TransformX(trx,try);	/* Get the screen x coord */
	px = ((tx*(int32_t)scale)>>7) + CENTERX;	/* Use 32 bit precision! */

/* calculate edges of the shape */

	patch = SpriteArray[thing->sprite];	/* Pointer to the sprite info */

	width =((LongWord)SwapUShortBE(patch[0])*scale)>>6; 	/* Get the width of the shape */
	if (!width) {
		return;		/* too far away*/
	}
	x1 = px - (width>>1);	/* Get the left edge */
	if (x1 >= (int) SCREENWIDTH) {
		return;		/* off the right side */
	}
	x2 = x1 + width - 1;			/* Get the right edge */
	if (x2 < 0) {
		return;		/* off the left side*/
	}
	VisPtr = &vissprites[numvisspr];
	VisPtr->pos = patch;	/* Sprite info offset */
	VisPtr->x1 = x1;			/* Min x */
	VisPtr->x2 = x2;			/* Max x */
	VisPtr->clipscale = scale;	/* Size to draw */
	VisPtr->columnstep = (SwapUShortBE(patch[0])<<8)/width; /* Step for width scale */
	VisPtr->actornum = actornum;	/* Actor who this is (0 for static) */

/* pack the vissprite number into the low 6 bits of the scale for sorting */

	xevents[numvisspr] = VisPtr;		/* Pass the scale in the upper 10 bits */
	++numvisspr;		/* 1 more valid record */
}

/**********************************

	Draw a scaling game over sprite on top of everything

**********************************/

void DrawTopSprite(void)
{
	unsigned short *patch;
	int x1, x2;
	Word width;
	vissprite_t VisRecord;

	if (topspritescale) {		/* Is there a top sprite? */

/* calculate edges of the shape */

		patch = SpriteArray[topspritenum];		/* Get the info on the shape */

		width = (SwapUShortBE(patch[0])*topspritescale)>>7;		/* Adjust the width */
		if (!width) {
			return;		/* Too far away */
		}
		x1 = CENTERX - (width>>1);		/* Use the center to get the left edge */
		if (x1 >= SCREENWIDTH) {
			return;		/* off the right side*/
		}
		x2 = x1 + width - 1;		/* Get the right edge */
		if (x2 < 0) {
			return;		/* off the left side*/
		}
		VisRecord.pos = patch;	/* Index to the shape record */
		VisRecord.x1 = x1;			/* Left edge */
		VisRecord.x2 = x2;			/* Right edge */
		VisRecord.clipscale = topspritescale;	/* Size */
		VisRecord.columnstep = (SwapUShortBE(patch[0])<<8)/(x2-x1+1);	/* Width step */
		VisRecord.actornum = -1;

/* Make sure it is sorted to be drawn last */

		memset(xscale,0,sizeof(xscale));		/* don't clip behind anything */
		RenderSprite(&VisRecord);		/* Draw the sprite */
	}
}

/**********************************

	Draw all the character sprites

**********************************/

void DrawSprites(void)
{
	Word i,j;				/* Index */
	static_t *stat;			/* Pointer to static sprite record */
	actor_t	*actor;			/* Pointer to active actor record */
	missile_t *MissilePtr;	/* Pointer to active missile record */

	numvisspr = 0;			/* Init the sprite count */

/* add all sprites in visareas*/

	if (numstatics) {		/* Any statics? */
		i = numstatics;
		stat = statics;			/* Init my pointer */
		do {
			if (areavis[stat->areanumber]) {	/* Is it in a visible area? */
				AddSprite((thing_t *) stat,0);	/* Add to my list */
			}
			++stat;		/* Next index */
		} while (--i);	/* Count down */
	}

	if (numactors>1) {		/* Any actors? */
		i = 1;				/* Index to the first NON-PLAYER actor */
		actor = &actors[1];	/* Init pointer */
		do {
			if (areavis[actor->areanumber]) {	/* Visible? */
				AddSprite ((thing_t *)actor, i);	/* Add it */
			}
			++actor;		/* Next actor */
		} while (++i<numactors);	/* Count up */
	}

	if (nummissiles) {		/* Any missiles? */
		i = nummissiles;	/* Get the missile count */
		MissilePtr = missiles;	/* Get the pointer to the first missile */
		do {
			if (areavis[MissilePtr->areanumber]) {	/* Visible? */
				AddSprite((thing_t *)MissilePtr,0);	/* Show it */
			}
			++MissilePtr;	/* Next missile */
		} while (--i);		/* Count down */
	}

	i = numvisspr;
	if (i) {			/* Any sprites? */

/* sort sprites from back to front*/

		SimpleSort();

/* draw from smallest scale to largest */

		j = 0;
		do {
			RenderSprite(xevents[j++]);	/* Draw the sprite */
		} while (--i);
	}
}
