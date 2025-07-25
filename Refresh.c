#include "WolfDef.h"
#include <string.h>
#include <SDL3/SDL_stdinc.h>

#define	DOORPIC	59

/* Scale ranges from 0xffff (one tile == 512 pixels high) down to 0x100 (one tile == 2 pixels) */

savenode_t *nodes;
saveseg_t *pwallseg;				/* pushwall in motion*/

/*
================================================

				MATH ROUTINES

================================================
*/

/* -8.8 * -0.8 = -8.8*/
fixed_t	FixedByFrac (fixed_t a, fixed_t b)
{
	fixed_t	c;
	c = ((int32_t)a * (int32_t)b)>>FRACBITS;
	return c;
}

/* -8.8 * -0.8 = -8.8*/
fixed_t	SUFixedMul (fixed_t a, ufixed_t b)
{
	fixed_t	c;
	c = ((int32_t)a * (int32_t)b)>>FRACBITS;
	return c;
}

/* -8.8 / -8.8 = -8.8*/
fixed_t	FixedDiv (fixed_t a, fixed_t b)
{
	a = (int32_t)(((int32_t)a<<FRACBITS) / (int32_t)b);
	return a;
}

/**********************************

	Return the x coord for a sprite

**********************************/

fixed_t R_TransformX(fixed_t x,fixed_t y)
{
	fixed_t gxt,gyt;
	gxt = FixedByFrac(x,viewsin);
	gyt = FixedByFrac(y,viewcos);
	return gxt-gyt;
}

/**********************************

	Return the scale factor for a sprite

**********************************/

fixed_t R_TransformZ(fixed_t x,fixed_t y)
{
	fixed_t	gxt,gyt;
	gxt = FixedByFrac(x,viewcos);
	gyt = FixedByFrac(y,viewsin);
	return gxt+gyt;
}

/**********************************

	Return the scale factor from a 64k range angle

**********************************/

Word ScaleFromGlobalAngle(short visangle,short distance)
{
	int32_t	tz;

	Word anglea, angleb;
	Word sinea, sineb;

	visangle >>= ANGLETOFINESHIFT;
	anglea = (FINEANGLES/4 + (visangle-centerangle))&(FINEANGLES/2-1);
	angleb = (FINEANGLES/4 + (visangle-normalangle))&(FINEANGLES/2-1);
	sinea = finesine[anglea];	/* bothe sines are always positive*/
	sineb = finesine[angleb];

	tz = ((int32_t)distance * sinea) / sineb;

	if (tz < 0)
		tz = 0;
	else if (tz>=MAXZ)		/* Make sure it's not too big... */
		tz = MAXZ-1;
	return scaleatzptr[tz];		/* Return the size */
}

/**********************************

	The automap has to be done in quadrants for various reasons

**********************************/

void DrawAutomap(Word tx,Word ty)
{
	Word i, tile;
	saveseg_t *seg;
	Word x,y,xstep,ystep,count;
	Word min,max;
	Word maxtx, maxty;
	Word NodeCount;

	NodeCount = MapPtr->numnodes;
	maxtx = tx+(SCREENWIDTH/16);
	maxty = ty+(SCREENHEIGHT/16);
	seg = (saveseg_t *)nodes;
	i = 0;
	do {
		if ( (seg->dir & (DIR_SEGFLAG|DIR_SEENFLAG)) == (DIR_SEGFLAG|DIR_SEENFLAG) ) {
			min = (seg->min-1)>>1;
			max = (seg->max+3)>>1;

			switch (seg->dir&3) {
			case di_north:
				x = (seg->plane-1)>>1;
				y = min;
				xstep = 0;
				ystep = 1;
				break;
			case di_south:
				x = seg->plane>>1;
				y = min;
				xstep = 0;
				ystep = 1;
				break;
			case di_east:
				y = (seg->plane-1)>>1;
				x = min;
				xstep = 1;
				ystep = 0;
				break;
			case di_west:
				y = seg->plane>>1;
				x = min;
				xstep = 1;
				ystep = 0;
				break;
			}

			for (count=max-min ; count ; --count,x+=xstep,y+=ystep) {
				if (x < tx || x >= maxtx || y < ty || y>=maxty) {
					continue;
				}
				tile = tilemap[y][x];
				if (tile & TI_DOOR) {
					tile = 59 /*(seg->dir&1) + 30*/;
				} else if (tile & TI_PUSHWALL) {
					if (ShowPush) {
						tile = 68;
					} else {
						tile = textures[y][x];
					}
				} else {
					tile = MapPtr->tilemap[y][x];
					if (! (tile&0x80) ) {
						continue;		/* not a solid tile*/
					}
					tile = textures[y][x];
				}
				DrawSmall(x-tx,y-ty,tile);	/* Draw the tile on the screen */
			}
		}
		++seg;
	} while (++i<NodeCount);
	x = viewx>>8;
	y = viewy>>8;
	if (x >= tx && x < maxtx && y >= ty && y<maxty) {
		tile = ((gamestate.viewangle+ANGLES/8*3)&(ANGLES-1)) >> 7;	/* Force into a cardinal direction */
		if (!(tile & 1))
			tile ^= 2;
		DrawSmall(x-tx,y-ty,tile+64);		/* Draw BJ's face here */
	}
	BlastScreen();
}

/**********************************

	Init my math tables for the current view

**********************************/

#define	PI	3.141592657

Boolean StartupRendering(Word NewSize)
{
#if 1
	int		i;
	Word minz;
	int		x;
	float	a, fv;
	int32_t	t;
	LongWord focallength;
	Word j;
	Word *ScalePtr;
#endif

	if (NewSize == MathSize) {	/* Already loaded? */
		return TRUE;
	}

#if 1		/* Only the mac version will calculate the tables */

/* generate scaleatz*/

	ScalePtr = scaleatzptr;
	minz = PROJECTIONSCALE/MAXFRAC;		/* What's the largest index */
	for (j=0;j<=minz;j++) {
		ScalePtr[j] = MAXFRAC;		/* Fill in the overflow area (No longs) */
	}
	do {					/* Calculate the rest */
		ScalePtr[j] = PROJECTIONSCALE/j;	/* Create whole numbers */
	} while (++j<MAXZ);

/* viewangle tangent table*/

	if (MathSize==(Word)-1) {		/* Only needs to be done once */
		i = 0;
		do {
			a = (i-(FINEANGLES>>2)+0.1)*PI*2/FINEANGLES;
			fv = 256*SDL_tanf(a);
			if (fv>0x7fff) {
				t = 0x7fff;
			} else if (fv<-0x7fff) {
				t = -0x7fff;
			} else {
				t = fv;
			}
			finetangent[i] = t;
		} while (++i<FINEANGLES/2);

/* finesine table*/

		i = 0;
		do {
			a = (i+0.0)*PI*2/FINEANGLES;
			t = 256*SDL_sinf(a);
			if (t>255) {
				t = 255;
			}
			if (t<1) {
				t = 1;
			}
			finesine[i] = t;
		} while (++i<FINEANGLES/2);
	}

/* use tangent table to generate viewangletox*/

	/* calc focallength so FIELDOFVIEW angles covers SCREENWIDTH */

	focallength = (SCREENWIDTH/2)<<FRACBITS/finetangent[FINEANGLES/4+FIELDOFVIEW/2];
	i = 0;
	do {
		t = ((int32_t) finetangent[i]*(int32_t)focallength)>>FRACBITS;
		t = (int)SCREENWIDTH/2 - t;
		if (t < -1) {
			t = -1;
		} else if (t>(int)SCREENWIDTH+1) {
			t = SCREENWIDTH+1;
		}
		viewangletox[i] = t;
	} while (++i<FINEANGLES/2);

/* scan viewangletox[] to generate xtoviewangle[]*/

	x = 0;
	do {
		i = 0;
		while (viewangletox[i]>=x) {
			i++;
		}
		xtoviewangle[x] = i-FINEANGLES/4-1;
	} while (++x<=SCREENWIDTH);

/* take out the fencepost cases from viewangletox*/

	for (i=0 ; i<FINEANGLES/2 ; i++) {
		t = SUFixedMul (finetangent[i], focallength);
		t = (int)SCREENWIDTH/2 - t;
		if (viewangletox[i] == -1) {
			viewangletox[i] = 0;
		} else if (viewangletox[i] == SCREENWIDTH+1) {
			viewangletox[i] = SCREENWIDTH;
		}
	}

#if 0		/* Should I save these tables? */
if (!NewSize) {
SaveJunk(scaleatzptr,sizeof(Word)*MAXZ);
SaveJunk(finetangent,sizeof(short)*FINEANGLES/2);
SaveJunk(finesine,sizeof(short)*FINEANGLES/2);
SaveJunk(viewangletox,sizeof(short)*FINEANGLES/2);
SaveJunk(xtoviewangle,sizeof(short)*(SCREENWIDTH+1));
GoodBye();
}
#endif

#else
/* All other versions load the tables from disk (MUCH FASTER!!) */
	if (MathSize==-1) {
		finetangent = LoadAResource(rFineTangent);
		finesine = LoadAResource(rFineSine);
	}
	scaleatzptr = LoadAResource(rScaleAtZ);
	viewangletox = LoadAResource(rViewAngleToX);
	xtoviewangle = LoadAResource(rXToViewAngle);
#endif

	clipshortangle = xtoviewangle[0]<<ANGLETOFINESHIFT;	/* Save leftmost angle for view */
	clipshortangle2 = clipshortangle*2;					/* Double the angle for constant */

/* textures for doors stay the same across maps*/

	memset(textures[128],DOORPIC+4,MAPSIZE); /* door side*/
	memset(textures[129],DOORPIC+0,MAPSIZE); /* regular door*/
	memset(textures[130],DOORPIC+1,MAPSIZE); /* lock 1*/
	memset(textures[131],DOORPIC+2,MAPSIZE); /* lock 2*/
	memset(textures[132],DOORPIC+3,MAPSIZE); /* elevator*/
	ReleaseScalers();		/* Release any compiled scalers */
	if (!SetupScalers()) {				/* Redo any scalers */
		return FALSE;
	}
	MathSize = NewSize;
	return TRUE;
}

/**********************************

	Alert the rendering engine about a new map

**********************************/

void NewMap(void)
{
	Word x,y, tile;
	Byte *src;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	savenode_t *FixPtr;
#endif
	memset(textures,0xff,128*64);	/* clear array so pushwall spawning can insert*/
									/* texture numbers in empty spots*/
	src = &MapPtr->tilemap[0][0];
	y = 0;
	do {
		x = 0;
		do {
			tile = *src++;
			if (tile&TI_BLOCKMOVE) {	/* blocking?*/
				tile = ((tile&0x3f)-1)*2;
				textures[MAPSIZE+x][y] = tile + 1;	/* Use the dark shape */
				textures[y][x] = tile;			/* Use the light shape */
			}
		} while (++x<MAPSIZE);
	} while (++y<MAPSIZE);

	nodes = (savenode_t *)((Byte *)MapPtr + MapPtr->nodelistofs);
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	y = MapPtr->numnodes;
	FixPtr = nodes;
	while (y) {
		if (!(FixPtr->dir & DIR_SEGFLAG)) {
			FixPtr->children[0] = SwapUShortLE(FixPtr->children[0]);	/* Swap endian on all offsets */
			FixPtr->children[1] = SwapUShortLE(FixPtr->children[1]);
		}
		++FixPtr;
		--y;
	}
#endif
	pwallseg = 0;		/* No pushwalls in progress */

}

/**********************************

	Set up my pushwall record so the BSP records will follow

**********************************/

void StartPushWall(void)
{
	Word i;
	Word segdir,segplane,segmin;
	saveseg_t *SavePtr;				/* Temp pointer */

	pwallseg = 0;	/* No pushwalls in progress */

	switch (PushWallRec.pwalldir) {	/* Which direction? */
	case CD_NORTH:
		segmin = PushWallRec.pwallx<<1;	/* Minimum segment */
		segplane = (PushWallRec.pwally+1)<<1;	/* y facing plane */
		segdir = di_east;	/* Point east */
		break;
	case CD_EAST:
		segplane = PushWallRec.pwallx<<1;	/* Minimum segment */
		segmin = PushWallRec.pwally<<1;
		segdir = di_south;	/* Point south */
		break;
	case CD_SOUTH:
		segmin = PushWallRec.pwallx<<1;
		segplane = PushWallRec.pwally<<1;
		segdir = di_west;	/* Point west */
		break;
	case CD_WEST:
		segplane = (PushWallRec.pwallx+1)<<1;
		segmin = PushWallRec.pwally<<1;
		segdir = di_north;	/* Point north */
		break;
	default:
		__builtin_unreachable();
	}

	SavePtr = (saveseg_t *)nodes;	/* Init pointer to the nodes */
	i = MapPtr->numnodes;		/* Get the node count */
	if (i) {				/* Maps MUST have nodes */
		do {
			if ((SavePtr->dir & DIR_SEGFLAG) &&		/* Segmented? */
				((SavePtr->dir & 3) == segdir) &&	/* Proper side? */
				(SavePtr->plane == segplane) &&		/* Proper plane? */
				(SavePtr->min == segmin) ) {		/* Proper segment */
					pwallseg = SavePtr;				/* Save the segment */
					return;							/* Exit */
			}
			++SavePtr;			/* Next index */
		} while (--i);			/* Count down */
	}
}

/**********************************

	Mark a pushwall BSP segment as disabled

**********************************/

void AdvancePushWall(void)
{
	if (pwallseg) {		/* Failsafe */
		pwallseg->dir |= DIR_DISABLEDFLAG;		/* Mark the pushwall as disabled */
	}
}

/**********************************

	Render the entire 3-D view

**********************************/

void RenderView(void)
{

	Word frame;

	centerangle = gamestate.viewangle<<GAMEANGLETOFINE;	/* 512 to 2048 */
	centershort = centerangle<<ANGLETOFINESHIFT;	/* 2048 to 64k */
	viewsin = sintable[gamestate.viewangle];	/* Get the basic sine */
	viewcos = costable[gamestate.viewangle];	/* Get the basic cosine */
	memset(areavis, 0, sizeof(areavis));	/* No areas are visible */
	ClearClipSegs();			/* Clip first seg only to sides of screen */
	IO_ClearViewBuffer();		/* Erase to ceiling / floor colors*/

	bspcoord[BSPTOP] = 0;		/* The map is 64*64 */
	bspcoord[BSPBOTTOM] = 64*FRACUNIT;
	bspcoord[BSPLEFT] = 0;
	bspcoord[BSPRIGHT] = 64*FRACUNIT;

	RenderBSPNode(0);		/* traverse the BSP tree from the root */
	DrawSprites();			/* sort all the sprites in any of the rendered areas*/
	DrawTopSprite();		/* draw game over sprite on top of everything*/

	if (!NoWeaponDraw) {	/* Don't draw the weapon? */
		frame = gamestate.attackframe;
		if (frame == 4) {
			frame = 1;			/* drop back frame*/
		}
		IO_AttackShape(gamestate.weapon*4 + frame);		/* Draw the gun shape */
	}
	IO_DisplayViewBuffer();	/* blit to screen */
}
