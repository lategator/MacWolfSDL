/* enmove.c*/
#include "WolfDef.h"

dirtype opposite[9] =
	{west,southwest,south,southeast,east,northeast,north,northwest,nodir};

dirtype diagonal[9][9] = {
/* east */	{nodir,nodir,northeast,nodir,nodir,nodir,southeast,nodir,nodir},
			{nodir,nodir,nodir,nodir,nodir,nodir,nodir,nodir,nodir},
/* north */ {northeast,nodir,nodir,nodir,northwest,nodir,nodir,nodir,nodir},
			{nodir,nodir,nodir,nodir,nodir,nodir,nodir,nodir,nodir},
/* west */	{nodir,nodir,northwest,nodir,nodir,nodir,southwest,nodir,nodir},
			{nodir,nodir,nodir,nodir,nodir,nodir,nodir,nodir,nodir},
/* south */ {southeast,nodir,nodir,nodir,southwest,nodir,nodir,nodir,nodir},
			{nodir,nodir,nodir,nodir,nodir,nodir,nodir,nodir,nodir},
			{nodir,nodir,nodir,nodir,nodir,nodir,nodir,nodir,nodir}
};

/**********************************

	Changes actor to a new state, setting ticcount to the max for that state

**********************************/

void NewState(actor_t *ActorPtr,stateindex_t state)
{
	state_t *StatePtr;
	StatePtr = &states[state];	/* Get the state record pointer */
	ActorPtr->state = state;		/* Set the actor's state */
	ActorPtr->ticcount = StatePtr->tictime;	/* Set the initial tick value */
	ActorPtr->pic = StatePtr->shapenum;	/* Set the current shape number */
}

/**********************************

	Attempts to move actor in its current (ActorPtr->dir) direction.

	If blocked by either a wall or an actor returns FALSE

	If move is either clear or blocked only by a door, returns TRUE and sets

	ActorPtr->tilex = new destination
	ActorPtr->tiley
	ActorPtr->areanumber = the floor tile number (0-(MAXAREAS-1)) of destination
	ActorPtr->distance = TILEGLOBAL, or doornumber if a door is blocking the way

	If a door is in the way, an OpenDoor call is made to start it opening.
	The actor code should wait until
 	doorobjlist[ActorPtr->distance].action = dr_open, meaning the door has been
	fully opened

**********************************/

/**********************************

	Check if I can move in a diagonal direction

**********************************/

Boolean CheckDiag(Word x,Word y)
{
	/* anything blocking stops diagonal move*/
	if (tilemap[y][x]&(TI_BLOCKMOVE|TI_ACTOR)) {
		return FALSE;		/* It's blocked */
	}
	return TRUE;		/* It's open! */
}

/**********************************

	Check if I can move in a sideways direction
	also do the code for an actor to open a door
	return 0 if blocked, 1 if open, 2 if I need to open a door

**********************************/

Word CheckSide(Word x,Word y,actor_t *ActorPtr)
{
	Word temp;

	temp=tilemap[y][x];		/* Get the tile */

	if (temp & TI_DOOR) {	/* Door? */
		if (!(temp&TI_BLOCKMOVE)) {	/* Not blocked? */
			return 1;	/* door is open*/
		}
		if (ActorPtr->class == CL_DOG)	{
			return 0;	/* dogs can't open doors */
		}
		temp = doors[temp&TI_NUMMASK].info>>1;
		if (temp==1 || temp==2) {
			return 0;
		}
		return 2;		/* I have to open the door */
	}
	if (temp&(TI_BLOCKMOVE|TI_ACTOR)) {	/* Normally blocked? */
		return 0;		/* Can't go this way */
	}
	return 1;		/* I can go! */
}

/**********************************

	Try to move the actor around

**********************************/

Boolean TryWalk (actor_t *ActorPtr)
{
	Word x,y;
	Word Temp;
	Word *TileMapPtr;
	Word tile;

	x = ActorPtr->goalx;		/* Where is my goal x,y? */
	y = ActorPtr->goaly;

	switch (ActorPtr->dir) {	/* Go in my direction */
	case north:				/* Go n,s,e,w */
		--y;
		goto DoSide;

	case east:
		++x;
		goto DoSide;

	case south:
		++y;
		goto DoSide;

	case west:
		--x;
DoSide:
		Temp = CheckSide(x,y,ActorPtr);	/* See if I can move this way */
		if (!Temp) {		/* Not this way? */
			return FALSE;	/* Exit */
		}
		if (Temp==2) {		/* Door? */
			OpenDoor(&doors[tilemap[y][x]&TI_NUMMASK]);	/* Open the door */
			ActorPtr->flags |= (FL_WAITDOOR|FL_NOTMOVING);	/* Force the actor to pause */
			return TRUE;	/* I'm ok! */
		}
		break;		/* Continue */

	case northeast:
		if (!CheckDiag(x+1,y)) {
			return FALSE;
		}
		y--;
		goto FixEast;

	case southeast:
		if (!CheckDiag(x+1,y)) {
			return FALSE;
		}
		y++;
FixEast:
		if (!CheckDiag(x,y)) {
			return FALSE;
		}
		x++;
		if (!CheckDiag(x,y)) {
			return FALSE;
		}
		break;

	case southwest:
		if (!CheckDiag(x-1,y)) {
			return FALSE;
		}
		y++;
		goto FixWest;

	case northwest:
		if (!CheckDiag(x-1,y)) {
			return FALSE;
		}
		y--;
FixWest:
		if (!CheckDiag(x,y)) {
			return FALSE;
		}
		x--;
		if (!CheckDiag(x,y)) {
			return FALSE;
		}
	}

/* invalidate the move if moving onto the player */

	if (areabyplayer[MapPtr->areasoundnum[ActorPtr->areanumber]]) {
		if (w_abs(((x<<FRACBITS)|0x80) - actors[0].x) < MINACTORDIST
		&& w_abs(((y<<FRACBITS)|0x80) - actors[0].y) < MINACTORDIST) {
			return FALSE;
		}
	}

/* remove old actor marker*/

	tilemap[ActorPtr->goaly][ActorPtr->goalx] &= ~TI_ACTOR;

/* place new actor marker*/

	TileMapPtr = &tilemap[y][x];	/* Get pointer to cell */
	tile = TileMapPtr[0];			/* Get the cell */
	TileMapPtr[0] = tile | TI_ACTOR;	/* Mark with an actor */
	Temp = ActorPtr - &actors[0];
	MapPtr->tilemap[y][x] = Temp;	/* Save the current actor # */
	ActorPtr->goalx = x;
	ActorPtr->goaly = y;

	if (!(tile&TI_DOOR) ) {		/* doorways are not areas*/
		ActorPtr->areanumber = tile&TI_NUMMASK;
	}
	ActorPtr->distance = TILEGLOBAL;	/* Move across 1 whole tile */
	ActorPtr->flags &= ~(FL_WAITDOOR|FL_NOTMOVING);	/* I'm not waiting and I'm moving */

	return TRUE;		/* It's ok! */
}

/**********************************

	Attempts to choose and initiate a movement for actor that sends it towards
	the player while dodging

**********************************/

void SelectDodgeDir(actor_t *ActorPtr)
{
	int	deltax,deltay;
	Word i;
	Word absdx,absdy;
	dirtype dirtry[5];
	dirtype turnaround,tdir;

	turnaround=opposite[ActorPtr->dir];

	deltax = actors[0].x - ActorPtr->x;
	deltay = actors[0].y - ActorPtr->y;

/* arange 5 direction choices in order of preference*/
/* the four cardinal directions plus the diagonal straight towards*/
/* the player*/

	if (deltax>0) {
		dirtry[1]= east;
		dirtry[3]= west;
	} else {
		dirtry[1]= west;
		dirtry[3]= east;
	}

	if (deltay>0) {
		dirtry[2]= south;
		dirtry[4]= north;
	} else {
		dirtry[2]= north;
		dirtry[4]= south;
	}

/* randomize a bit for dodging*/

	absdx = w_abs(deltax);
	absdy = w_abs(deltay);

	if (absdx > absdy) {
		tdir = dirtry[1];
		dirtry[1] = dirtry[2];
		dirtry[2] = tdir;
		tdir = dirtry[3];
		dirtry[3] = dirtry[4];
		dirtry[4] = tdir;
	}

	if (w_rnd() & 1) {
		tdir = dirtry[1];
		dirtry[1] = dirtry[2];
		dirtry[2] = tdir;
		tdir = dirtry[3];
		dirtry[3] = dirtry[4];
		dirtry[4] = tdir;
	}

	dirtry[0] = diagonal[dirtry[1]][dirtry[2]];

/* try the directions until one works*/

	i = 0;
	do {
		tdir = dirtry[i];
		if (tdir != nodir && tdir != turnaround) {
			ActorPtr->dir = tdir;
			if (TryWalk(ActorPtr)) {	/* Can I go this way? */
				return;		/* Yep! */
			}
		}
	} while(++i<5);		/* All tries done? */

/* turn around only as a last resort*/

	if (turnaround != nodir) {
		ActorPtr->dir = turnaround;
		if (TryWalk(ActorPtr)) {
			return;
		}
	}
	ActorPtr->dir = nodir;		/* Stop the motion */
	ActorPtr->flags |= FL_NOTMOVING;	/* Kill the logic! */
}

/**********************************

	Attempts to choose and initiate a movement for actor that sends it towards
	the player but doesn't try to dodge

**********************************/

void SelectChaseDir(actor_t *ActorPtr)
{
	int deltax,deltay;
	dirtype d[3];
	dirtype tdir, olddir, turnaround;

	olddir = (dirtype) ActorPtr->dir;	/* Save the current direction */
	turnaround=opposite[olddir];	/* What's the opposite direction */

	deltax = actors[0].x - ActorPtr->x;	/* Which way to travel? */
	deltay = actors[0].y - ActorPtr->y;

	if (deltax>0) {
		d[1]= east;
	} else if (!deltax) {
		d[1]= nodir;
	} else {
		d[1]= west;
	}
	if (deltay>0) {
		d[2]=south;
	} else if (!deltay) {
		d[2]=nodir;
	} else {
		d[2]=north;
	}

	if (w_abs(deltay)>w_abs(deltax)) {	/* Swap if Y is greater */
		tdir=d[1];
		d[1]=d[2];
		d[2]=tdir;
	}

	if (d[1]==turnaround) {	/* Try not to turn around */
		d[1]=nodir;
	}
	if (d[2]==turnaround) {	/* Try not to turn around */
		d[2]=nodir;
	}

	if (d[1]!=nodir) {	/* East/West movement? */
		ActorPtr->dir = d[1];	/* Try to move */
		if (TryWalk(ActorPtr)) {
			return;	/*either moved forward or attacked*/
		}
	}

	if (d[2]!=nodir) {	/* North/South movement? */
		ActorPtr->dir =d[2];
		if (TryWalk(ActorPtr)) {
			return;
		}
	}

/* there is no direct path to the player, so pick another direction */

	if (olddir!=nodir) {
		ActorPtr->dir =olddir;		/* Continue in the old direction */
		if (TryWalk(ActorPtr)) {
			return;
		}
	}

	if (w_rnd()&1) {	/*randomly determine direction of search*/
		tdir = north;
		do {
			if (tdir!=turnaround) {
				ActorPtr->dir =tdir;
				if (TryWalk(ActorPtr)) {
					return;
				}
			}
		} while(++tdir<(west+1));
	} else {
		tdir = west;
		do {
			if (tdir!=turnaround) {
				ActorPtr->dir =tdir;
				if ( TryWalk(ActorPtr)) {
					return;
				}
			}
		} while(--tdir>=north);
	}

	if (turnaround != nodir) {	/* Alright, try backwards now */
		ActorPtr->dir =turnaround;
		if ( TryWalk(ActorPtr) ) {
			return;
		}
	}
	ActorPtr->dir = nodir;		/* Can't move, I give up */
	ActorPtr->flags |= FL_NOTMOVING;
}

/**********************************

	Moves actor <move> global units in actor->dir direction
	Actors are not allowed to move inside the player
	Does NOT check to see if the move is tile map valid

**********************************/

void MoveActor(actor_t *ActorPtr,Word move)
{
	Word tryx,tryy;

	tryx = ActorPtr->x;	/* Get the x and y */
	tryy = ActorPtr->y;

	switch (ActorPtr->dir) {;
	case northeast:		/* Move to the new x,y */
		tryx += move;
	case north:
		tryy -= move;
		break;

	case southeast:
		tryy += move;
	case east:
		tryx += move;
		break;

	case southwest:
		tryx -= move;
	case south:
		tryy += move;
		break;

	case northwest:
		tryy -= move;
	case west:
		tryx -= move;
	}

/* check to make sure it's not moving on top of player*/

	if (areabyplayer[MapPtr->areasoundnum[ActorPtr->areanumber]]) {
		if (w_abs(tryx - actors[0].x)<MINACTORDIST) {
			if (w_abs(tryy - actors[0].y)<MINACTORDIST) {
				return;
			}
		}
	}
	ActorPtr->distance-= move;	/* Remove the distance */
	ActorPtr->x = tryx;		/* Save the new x,y */
	ActorPtr->y = tryy;
}
