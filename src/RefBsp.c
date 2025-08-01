#include "WolfDef.h"

static Word checkcoord[11][4] = {	/* Indexs to the bspcoord table */
{3,0,2,1},
{3,0,2,0},
{3,1,2,0},
{0,0,0,0},
{2,0,2,1},
{0,0,0,0},		/* Not valid */
{3,1,3,0},
{0,0,0,0},
{2,0,3,1},
{2,1,3,1},
{2,1,3,0}};

/**********************************

	Draw a 3-D textured polygon, must be done FAST!

**********************************/

void RenderWallLoop(Word x1,Word x2,Word distance)
{
	fixed_t	texturecolumn;
	Word tile,scaler, angle;

/* calculate and draw each column */

	if (rw_downside) {
		while (x1 < x2) {		/* Time to draw? */
			scaler = rw_scale >> FRACBITS;		/* Get the draw scale */
			xscale[x1] = scaler;		/* Save the scale factor */
			angle = (xtoviewangle[x1]+rw_centerangle)%(FINEANGLES/2);
			texturecolumn = rw_midpoint - SUFixedMul(finetangent[angle],distance);	/* Which texture to use? */
			if ((Word)texturecolumn < rw_mintex) {
				texturecolumn = rw_mintex;
			} else if ((Word)texturecolumn >= rw_maxtex) {
				texturecolumn = rw_maxtex-1;
			}
			tile = rw_texture[texturecolumn>>8];	/* Get the tile to use */
			IO_ScaleWallColumn(x1,scaler,tile,(texturecolumn>>1)&127);	/* Draw the line */
			++x1;						/* Next x */
			rw_scale+=rw_scalestep;		/* Step the scale factor for the wall */
		}
		return;
	}
	while (x1 < x2) {		/* Time to draw? */
		scaler = rw_scale >> FRACBITS;		/* Get the draw scale */
		xscale[x1] = scaler;		/* Save the scale factor */
		angle = (xtoviewangle[x1]+rw_centerangle)%(FINEANGLES/2);
		texturecolumn = SUFixedMul(finetangent[angle],distance)+rw_midpoint;	/* Which texture to use? */
		if ((Word)texturecolumn < rw_mintex) {
			texturecolumn = rw_mintex;
		} else if ((Word)texturecolumn >= rw_maxtex) {
			texturecolumn = rw_maxtex-1;
		}
		tile = rw_texture[texturecolumn>>8];	/* Get the tile to use */
		if (!(WallList[tile+1] & 0x4000)) {
			texturecolumn^=0xff;	/* Reverse the tile for N,W walls */
		}
		IO_ScaleWallColumn(x1,scaler,tile,(texturecolumn>>1)&127);	/* Draw the line */
		++x1;						/* Next x */
		rw_scale+=rw_scalestep;		/* Step the scale factor for the wall */
	}
}


/*
=====================
=
= RenderWallRange
=
= Draw a wall segment between start and stop angles (inclusive) (short angles)
= No clipping is needed
=
======================
*/

void RenderWallRange (Word start,Word stop,saveseg_t *seg,Word distance)
{
	LongWord scale2;
	Word vangle;
	Word x1,x2;

/* mark the segment as visible for auto map*/

	seg->dir |= DIR_SEENFLAG;		/* for automap*/
	areavis[seg->area] = 1;			/* for sprite drawing*/

	start -= ANGLE180;		/* Adjust the start angle */
	stop -= ANGLE180;		/* Adjust the stop angle */
	vangle = (Word)(start+ANGLE90)>>ANGLETOFINESHIFT;
	x1 = viewangletox[vangle];
	vangle = (Word)(stop+ANGLE90-1)>>ANGLETOFINESHIFT;	/* make non inclusive*/
	x2 = viewangletox[vangle];
	if (x2 == x1) {
		return;		/* less than one column wide*/
	}
	rw_scale = (int32_t) ScaleFromGlobalAngle(start+centershort,distance)<<FRACBITS;
	if (x2>x1+1) {
		scale2 = (int32_t) ScaleFromGlobalAngle(stop+centershort,distance)<<FRACBITS;
		rw_scalestep = (int32_t)(scale2-rw_scale)/(int32_t)(x2-x1);
	}
	RenderWallLoop(x1,x2,distance);
}

/*
===============================================================================
=
= ClipWallSegment
=
= Clips the given screenpost and includes it in newcolumn
===============================================================================
*/

/* a screenpost_t is a solid range of visangles, used to clip and detect*/
/* span exposures / hidings*/

typedef	struct {
	Word top, bottom;
} screenpost_t;

#define	MAXSEGS	256

screenpost_t solidsegs[MAXSEGS], *newend;	/* newend is one past the last valid seg */

void ClipWallSegment(Word top,Word bottom,saveseg_t *seg,Word distance)
{
	screenpost_t *next, *start;

/* find the first clippost that touches the source post (adjacent pixels are touching)*/
	start = solidsegs;
	while (start->bottom > top+1) {
		start++;
	}

	if (top > start->top) {
		if (bottom > start->top+1) {	/* post is entirely visible (above start), so insert a new clippost*/
			RenderWallRange(top, bottom,seg,distance);
			if (newend >= solidsegs + MAXSEGS)
				return;
			next = newend;
			newend++;
			while (next != start) {
				*next = *(next-1);
				next--;
			}
			next->top = top;
			next->bottom = bottom;
			return;
		}

	/* there is a fragment above *start*/
		RenderWallRange (top, start->top + 1,seg,distance);
		start->top = top;		/* adjust the clip size*/
	}

	if (bottom >= start->bottom)
		return;			/* bottom contained in start*/

	next = start;
	while (bottom <= (next+1)->top+1) {
		/* there is a fragment between two posts*/
		RenderWallRange (next->bottom - 1, (next+1)->top + 1,seg,distance);
		next++;
		if (bottom >= next->bottom) {	/* bottom is contained in next*/
			start->bottom = next->bottom;	/* adjust the clip size*/
			goto crunch;
		}
	}

	/* there is a fragment after *next*/
	RenderWallRange (next->bottom - 1, bottom,seg,distance);
	start->bottom = bottom;		/* adjust the clip size*/


/* remove start+1 to next from the clip list, because start now covers their area*/
crunch:
	if (next == start) {
		return;			/* post just extended past the bottom of one post*/
	}
	while (next++ != newend)	/* remove a post*/
		*++start = *next;
	newend = start+1;
}

/**********************************

	Clear out the edge segments for the ray cast
	(Assume full viewing angle)

**********************************/

void ClearClipSegs(void)
{
	solidsegs[0].top = -1;		/* Maximum angle */
	solidsegs[0].bottom = ANGLE180 + clipshortangle;	/* First edge */
	solidsegs[1].top = ANGLE180 - clipshortangle;		/* Left edge */
	solidsegs[1].bottom = 0;		/* Minimum angle */
	newend = solidsegs+2;
}

/**********************************

	Clip and draw a given wall segment

**********************************/

void P_DrawSeg (saveseg_t *seg)
{
	Word	segplane;
	Word	door;
	door_t	*door_p;
	unsigned short	span, tspan;
	unsigned short	angle1, angle2;
	int		texslide;
	int		distance;

	if (seg->dir & DIR_DISABLEDFLAG) {	/* Segment shut down? */
		return;		/* pushwall part*/
	}

	segplane = (Word)seg->plane << 7;
	rw_mintex = (Word)seg->min << 7;
	rw_maxtex = (Word)seg->max << 7;

/* adjust pushwall segs */

	if (seg == pwallseg) {		/* Is this the active pushwall? */
		if (seg->dir&1)	{	/* east/west*/
			segplane += PushWallRec.pwallychange;
		} else {	/* north/south*/
			segplane += PushWallRec.pwallxchange;
		}
	}

/* get texture*/

	if (seg->texture >= 129) {	/* segment is a door */
		door = seg->texture - 129;	/* Which door is this? */
		if (door >= numdoors)
			return;
		door_p = &doors[door];
		rw_texture = &textures[129 + (door_p->info>>1)][0];
		texslide = door_p->position;
		rw_mintex += texslide;
	} else {
		texslide = 0;
		rw_texture = &textures[seg->texture][0];
	}

	switch (seg->dir&3) {	/* mask off the flags*/
	case di_north:
		distance = viewx - segplane;
		if (distance <= 0) {
			return;		/* back side*/
		}
		rw_downside = FALSE;
		rw_midpoint = viewy;
		normalangle = 2*FINEANGLES/4;
		angle1 = PointToAngle(segplane,rw_maxtex);
		angle2 = PointToAngle(segplane,rw_mintex);
		break;
	case di_south:
		distance = segplane - viewx;
		if (distance <= 0) {
			return;		/* back side*/
		}
		rw_downside = TRUE;
		rw_midpoint = viewy;
		normalangle = 0*FINEANGLES/4;
		angle1 = PointToAngle(segplane,rw_mintex);
		angle2 = PointToAngle(segplane,rw_maxtex);
		break;
	case di_east:
		distance = viewy - segplane;
		if (distance <= 0) {
			return;		/* back side*/
		}
		rw_downside = TRUE;
		rw_midpoint = viewx;
		normalangle = 1*FINEANGLES/4;
		angle1 = PointToAngle(rw_mintex,segplane);
		angle2 = PointToAngle(rw_maxtex,segplane);
		break;
	case di_west:
		distance = segplane - viewy;
		if (distance <= 0) {
			return;		/* back side*/
		}
		rw_downside = FALSE;
		rw_midpoint = viewx;
		normalangle = 3*FINEANGLES/4;
		angle1 = PointToAngle(rw_maxtex,segplane);
		angle2 = PointToAngle(rw_mintex,segplane);
		break;
	}

/* clip to view edges*/

	span = angle1 - angle2;
	if (span >= 0x8000) {		/* Test for negative (32 bit clean) */
		return;
	}
	angle1 -= centershort;
	angle2 -= centershort;
	++angle2;	/* make angle 2 non inclusive*/

	tspan = angle1 + clipshortangle;
	if (tspan > clipshortangle2) {
		tspan -= clipshortangle2;
		if (tspan >= span) {
			return;	/* totally off the left edge*/
		}
		angle1 = clipshortangle;
	}
	tspan = clipshortangle - angle2;
	if (tspan > clipshortangle2) {
		tspan -= clipshortangle2;
		if (tspan >= span) {
			return;	/* totally off the left edge*/
		}
		angle2 = -clipshortangle;
	}

/* calc center angle for texture mapping*/

	rw_centerangle = (centerangle-normalangle)&FINEMASK;
	if (rw_centerangle > FINEANGLES/2) {
		rw_centerangle -= FINEANGLES;
	}
	rw_centerangle += FINEANGLES/4;

	rw_midpoint -= texslide;
	rw_mintex -= texslide;

	angle1 += ANGLE180;		/* adjust so angles are unsigned*/
	angle2 += ANGLE180;
	ClipWallSegment(angle1, angle2,seg,distance);
}

/**********************************

	Returns True if some part of the BSP dividing line might be visible

**********************************/

Boolean CheckBSPNode(Word boxpos)
{
	short 	angle1, angle2;
	unsigned short	span, tspan;
	unsigned short	uangle1, uangle2;
	screenpost_t	*start;
	Word *PosPtr;
	int x1,y1,x2,y2;

	PosPtr = &checkcoord[boxpos][0];
	x1 = bspcoord[PosPtr[0]];
	y1 = bspcoord[PosPtr[1]];
	x2 = bspcoord[PosPtr[2]];
	y2 = bspcoord[PosPtr[3]];

	angle1 = PointToAngle(x1,y1) - centershort;
	angle2 = PointToAngle(x2,y2) - centershort;

/* check clip list for an open space */

	span = angle1 - angle2;
	if (span >= 0x8000) {
		return TRUE;	/* sitting on a line*/
	}
	tspan = angle1 + clipshortangle;
	if (tspan > clipshortangle2) {
		tspan -= clipshortangle2;
		if (tspan >= span) {
			return FALSE;	/* totally off the left edge*/
		}
		angle1 = clipshortangle;
	}
	tspan = clipshortangle - angle2;
	if (tspan > clipshortangle2) {
		tspan -= clipshortangle2;
		if (tspan >= span) {
			return FALSE;	/* totally off the left edge*/
		}
		angle2 = -clipshortangle;
	}


/* find the first clippost that touches the source post (adjacent pixels are touching)*/
	uangle1 = angle1 + ANGLE180;
	uangle2 = angle2 + ANGLE180;
	start = solidsegs;
	while (start->bottom > uangle1+1) {
		++start;
	}
	if (uangle1 <= start->top && uangle2 >= start->bottom) {
		return FALSE;	/* the clippost contains the new span*/
	}
	return TRUE;
}


/**********************************

	Draw one or more wall segments

**********************************/

void TerminalNode (saveseg_t *seg)
{
	for (;;) {				/* Forever? */
		P_DrawSeg(seg);		/* Draw the wall segment (If visible) */
		if (seg->dir & DIR_LASTSEGFLAG) {	/* Last segment in list? */
			return;			/* Exit now */
		}
		++seg;				/* Index to the next wall segment */
	}
}

/**********************************

	Render the 3D view by recursivly following the BSP tree

**********************************/

void RenderBSPNode(Word bspnum)
{
	savenode_t *bsp;	/* Pointer to the current BSP node */
	Word side;			/* decision line */
	Word coordinate;	/* Current coord */
	Word savednum;		/* Save index */
	Word savedcoordinate;	/* Save value */
	Word boxpos;		/* Index to the coord table */

	bsp = &nodes[bspnum];		/* Get pointer to the current tree node */
	if (bsp->dir & DIR_SEGFLAG) {		/* There is a segment here... */
		TerminalNode((saveseg_t *)bsp);	/* Render it */
		return;							/* Exit */
	}

/* decision node */

	coordinate = bsp->plane<<7;		/* stored as half tiles*/

	if (bsp->dir) {					/* True for vertical tiles */
		side = viewx > coordinate;	/* vertical decision line*/
		savednum = BSPLEFT + (side^1);	/* Left or right */
	} else {
		side = viewy > coordinate;	/* horizontal decision line*/
		savednum = BSPTOP + (side^1);	/* Top or bottom */
	}

	savedcoordinate = bspcoord[savednum];	/* Save this coord */
	bspcoord[savednum] = coordinate;		/* Set my new coord boundary */
	RenderBSPNode(bsp->children[side^1]);	/* recursively divide front space*/

	bspcoord[savednum] = savedcoordinate;	/* Restore the coord */
	savednum ^= 1;							/* Negate the index */
	savedcoordinate = bspcoord[savednum];	/* Save the other side */
	bspcoord[savednum] = coordinate;		/* Set the new side */

/* if the back side node is a single seg, don't bother explicitly checking visibility */

	if ( ! ( nodes[bsp->children[side]].dir & DIR_LASTSEGFLAG ) ) {

	/* don't flow into the back space if it is not going to be visible */

		if (viewx <= bspcoord[BSPLEFT]) {
			boxpos = 0;
		} else if (viewx < bspcoord[BSPRIGHT]) {
			boxpos = 1;
		} else {
			boxpos = 2;
		}
		if (viewy > bspcoord[BSPTOP]) {
			if (viewy < bspcoord[BSPBOTTOM]) {
				boxpos += 4;
			} else {
				boxpos += 8;
			}
		}
		if (!CheckBSPNode(boxpos)) {	/* Intersect possible? */
			goto skipback;				/* Exit now then */
		}
	}
	RenderBSPNode(bsp->children[side]);	/* recursively divide back space */
skipback:
	bspcoord[savednum] = savedcoordinate;
}
