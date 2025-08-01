#include "WolfDef.h"

#define PWALLSPEED 4		/* Micropixels per 60th of a second */

pushwall_t PushWallRec;				/* Record for the single pushwall in progress */

/**********************************

	Set pwallychange and pwallxchange
	Uses pwallpos,pwalldir

**********************************/

void SetPWallChange(void)
{
	Word pos;

	pos = PushWallRec.pwallpos&(FRACUNIT-1);	/* Get the pixel fraction */
	PushWallRec.pwallxchange = 0;		/* No motion yet */
	PushWallRec.pwallychange = 0;
	switch (PushWallRec.pwalldir) {		/* Which way? */
	case CD_NORTH:
		PushWallRec.pwallychange = -pos;		/* Y motion */
		return;
	case CD_EAST:
		PushWallRec.pwallxchange = pos;			/* X motion */
		return;
	case CD_SOUTH:
		PushWallRec.pwallychange = pos;			/* Y motion */
		return;
	case CD_WEST:
		PushWallRec.pwallxchange = -pos;		/* X motion */
	}
}

/**********************************

	Marks the dest tile as blocked and begins a push wall sequence
	Uses pwallx,pwally,pwalldir

**********************************/

void PushWallOne(void)
{
	PushWallRec.pwallcheckx = PushWallRec.pwallx;
	PushWallRec.pwallchecky = PushWallRec.pwally;

	switch (PushWallRec.pwalldir) {
	case CD_NORTH:
		PushWallRec.pwallchecky--;
		break;
	case CD_EAST:
		PushWallRec.pwallcheckx++;
		break;
	case CD_SOUTH:
		PushWallRec.pwallchecky++;
		break;
	case CD_WEST:
		PushWallRec.pwallcheckx--;
	}
	tilemap[PushWallRec.pwallchecky][PushWallRec.pwallcheckx] |= TI_BLOCKMOVE | TI_BLOCKSIGHT;
	StartPushWall();	/* let the refresh do some junk*/
}

/**********************************

	Initiate a pushwall sequence
	Call with x,y,dir of wall to push

**********************************/

void PushWall(Word x,Word y,Word dir)
{
	PlaySound(SND_PWALL);	/* Play the wall sound */
	PushWallRec.pwallx = x;			/* Save the x,y in my globals */
	PushWallRec.pwally = y;
	PushWallRec.pwalldir = dir;		/* Save the wall direction */
	PushWallOne();		/* Initiate the animation */
	PushWallRec.pwallcount = 2;		/* Move two cells */
	++gamestate.secretcount;	/* Secret area found */
	PushWallRec.pwallpos = PWALLSPEED/2;	/* Begin the move */

/* mark the segs that are being moved */

	tilemap[y][x] &= ~TI_PUSHWALL;	/* Clear the pushwall bit */
	SetPWallChange();		/* Set pwallchange */
}

/**********************************

	Continue any pushwall animations, called by the main loop

**********************************/

void MovePWalls (void)
{
	if (!PushWallRec.pwallcount) {	/* Any walls to push? */
		return;			/* Nope, get out */
	}

	PushWallRec.pwallpos += PWALLSPEED*TicCount;	/* Move the wall a little */
	SetPWallChange();		/* Set the wall for the renderer */
	if (PushWallRec.pwallpos<256) {		/* Crossed a tile yet? */
		return;				/* Exit now */
	}
	PushWallRec.pwallpos -= 256;		/* Mark as crossed */

	/* the tile can now be walked into */

	tilemap[PushWallRec.pwally][PushWallRec.pwallx] &= ~TI_BLOCKMOVE;	/* The movable block is gone */
	AdvancePushWall();		/* Remove the bsp seg */

	if (!--PushWallRec.pwallcount) {	/* Been pushed 2 blocks?*/
		StopSound(SND_PWALL);	/* Play the wall sound */
		PlaySound(SND_PWALL2);	/* Play the wall stop sound */
		return;				/* Don't do this anymore! */
	}
	PushWallRec.pwallx = PushWallRec.pwallcheckx;	/* Set the tile to the next cell */
	PushWallRec.pwally = PushWallRec.pwallchecky;
	PushWallOne();			/* Push one more record */
}