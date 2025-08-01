#include "WolfDef.h"
#include <string.h>

/**********************************

	Rules for door operation

door->position holds the amount the door is open, ranging from 0 to TILEGLOBAL-1

The number of doors is limited to 64 because various fields are only 6 bits

Open doors conect two areas, so sounds will travel between them and sight
	will be checked when the player is in a connected area.

areaconnect has a list of connected area #'s, used to create the table areabyplayer

Every time a door opens or closes the areabyplayer matrix gets recalculated.
	An area is True if it connects with the player's current spor.

**********************************/

/**********************************

	Insert a connection between two rooms
	Note: I can have MORE than one connection between rooms
	so it is VALID to have duplicate entries (1,6) & (1,6)
	Each call to AddConnection must be balanced with a call to RemoveConnection

**********************************/

void AddConnection(Word Area1,Word Area2)
{
	connect_t *DestPtr;

	DestPtr = &areaconnect[ConnectCount];	/* Make pointer to the last record */
	DestPtr->Area1 = Area1;					/* Init the struct */
	DestPtr->Area2 = Area2;
	++ConnectCount;							/* Add 1 to the valid list */
}

/**********************************

	Remove a connection between two rooms
	Note: I can have MORE than one connection between rooms
	so it is VALID to have duplicate entries (1,6) & (1,6)

**********************************/

void RemoveConnection(Word Area1,Word Area2)
{
	Word i;
	connect_t *DestPtr;

	DestPtr = &areaconnect[0];		/* Init the scan pointer */
	i = ConnectCount;				/* Init the count */
	if (!i) {
		return;
	}
	do {
		if (DestPtr->Area1 == Area1 &&	/* Match? */
			DestPtr->Area2 == Area2) {
			--ConnectCount;			/* Remove the count */
			DestPtr[0] = areaconnect[ConnectCount];	/* Copy last to current */
			break;			/* I'm done! */
		}
		++DestPtr;		/* Next entry to scan */
	} while (--i);		/* Should NEVER fall out! */
}

/**********************************

	Recursive routine to properly set the areabyplayer array by
	using the contents of the areaconnect array.
	Scans outward from playerarea, marking all connected areas.

**********************************/

void RecursiveConnect(Word areanumber)
{
	Word i;
	Word j;
	connect_t *AreaPtr;

	areabyplayer[areanumber] = TRUE;	/* Mark this spot (Prevent overflow) */

	i = ConnectCount;		/* Init index */
	if (i) {				/* No doors available? */
		AreaPtr = &areaconnect[0];	/* Get a local pointer */
		do {
			if (AreaPtr->Area1 == areanumber) {		/* Am I in this pair? */
				j = AreaPtr->Area2;			/* Follow this path */
				goto TryIt;
			}
			if (AreaPtr->Area2 == areanumber) {		/* The other side? */
				j = AreaPtr->Area1;			/* Follow this side */
TryIt:
				if (!areabyplayer[j]) {		/* Already been here? */
					RecursiveConnect(j);	/* Link it in... */
				}
			}
			++AreaPtr;				/* Next entry */
		} while (--i);	/* All done? */
	}
}

/**********************************

	Properly set the areabyplayer record

**********************************/

void ConnectAreas(void)
{
	memset(areabyplayer,0,sizeof(areabyplayer));		/* Zap the memory */
	RecursiveConnect(MapPtr->areasoundnum[actors[0].areanumber]);	/* Start here */
}

/**********************************

	Start a door opening

**********************************/

void OpenDoor(door_t *door)
{
	if (door->action == DR_OPEN) {	/* Already open? */
		door->ticcount = 0;			/* Reset open time (Keep open) */
	} else {
		door->action = DR_OPENING;	/* start it opening*/
	}	/* The door will be made passable when it is totally open */
}

/**********************************

	Start a door closing

**********************************/

void CloseDoor(door_t *door)
{
	Word tile,tilex,tiley;
	Word *TilePtr;
	int delta;
	Word area1,area2;
	Byte *SoundNumPtr;

/* don't close on anything solid */

	tilex = door->tilex;		/* Get the current tile */
	tiley = door->tiley;
	TilePtr = &tilemap[tiley][tilex];	/* Get pointer to tile map */

	if (door->action != DR_OPENING) {	/* In the middle of opening? */

/* don't close on an actor or bonus item */

		tile = TilePtr[0];		/* What's the tile? */
		if (tile & TI_BODY) {
			door->action = DR_WEDGEDOPEN;	/* bodies never go away */
			return;
		}
		if (tile & (TI_ACTOR | TI_GETABLE) ) {	/* Removable? */
			door->ticcount = 60;		/* wait a while before trying to close again */
			return;
		}

/* Don't close on the player */

		delta = actors[0].x - ((tilex<<8)|0x80);
		if (w_abs(delta) <= (0x82+PLAYERSIZE)) {
			delta = actors[0].y - ((tiley<<8)|0x80);
			if (w_abs(delta) <= (0x82+PLAYERSIZE)) {
				return;		/* It's touching the player! */
			}
		}
	}
	SoundNumPtr = MapPtr->areasoundnum;	/* Unlink the sound areas */
	area1 = SoundNumPtr[door->area1];
	area2 = SoundNumPtr[door->area2];
	if (areabyplayer[area1] || areabyplayer[area2]) {	/* Can I hear it? */
		PlaySound(SND_OPENDOOR);		/* Play the door sound */
	}
	door->action = DR_CLOSING;	/* Close the door */
	TilePtr[0] |= (TI_BLOCKMOVE|TI_BLOCKSIGHT);	/* make the door space a solid tile*/
}

/**********************************

	Open or Close a door (Press space at a door)

**********************************/

void OperateDoor(Word dooron)
{
	Word type;
	door_t *door;

	door = &doors[dooron];	/* Which door? */
	type = door->info>>1;	/* Get the door type */

	if ( (type==1 && !(gamestate.keys&1)) || (type==2 && !(gamestate.keys&2)) ) {
		PlaySound(SND_LOCKEDDOOR);		/* The door is locked */
		return;
	}

	switch (door->action) {
	case DR_CLOSED:
	case DR_CLOSING:
		OpenDoor(door);		/* Open the door */
		break;
	case DR_OPEN:
	case DR_OPENING:
		CloseDoor(door);	/* Close the door */
	}
}

/**********************************

	Close the door after a few seconds

**********************************/

void DoorOpen(door_t *door)
{
	door->ticcount+=TicCount;		/* Inc the tic value */
	if (door->ticcount >= OPENTICS) {	/* Time up? */
		door->ticcount = OPENTICS-1;	/* So if the door can't close it will keep trying*/
		CloseDoor(door);	/* Try to close the door */
	}
}

/**********************************

	Step the door animation for open, mark as DR_OPEN if all the way open

**********************************/

void DoorOpening(door_t *door)
{
	Word position;
	Word area1,area2;
	Byte *SoundNumPtr;

	position = door->position;	/* Get the pixel position */

	if (!position) {			/* Fully closed? */

	/* door is just starting to open, so connect the areas*/
		SoundNumPtr = MapPtr->areasoundnum;
		area1 = SoundNumPtr[door->area1];
		area2 = SoundNumPtr[door->area2];
		AddConnection(area1,area2);		/* Link the two together */
		ConnectAreas();					/* Link the map */
		if (areabyplayer[area1] || areabyplayer[area2]) {	/* Can I hear it? */
			PlaySound(SND_OPENDOOR);		/* Play the door sound */
		}
	}

/* slide the door open a bit */

	position += DOORSPEED*TicCount;		/* Move the door a bit */
	if (position >= TILEGLOBAL-1) {	/* Fully open? */

	/* Door is all the way open */

		position = TILEGLOBAL-1;	/* Set to maximum */
		door->ticcount = 0;			/* Reset the timer for closing */
		door->action = DR_OPEN;		/* Mark as open */
		tilemap[door->tiley][door->tilex] &= ~(TI_BLOCKMOVE|TI_BLOCKSIGHT);	/* Mark as enterable */
	}
	door->position = position;	/* Set the new position */
}

/**********************************

	Step the door animation for close,
	mark as DR_CLOSED if all the way closed

**********************************/

void DoorClosing(door_t *door)
{
	int position;
	Byte *SoundNumPtr;

	if (tilemap[door->tiley][door->tilex] & (TI_BODY|TI_GETABLE|TI_ACTOR)) {
		OpenDoor(door);
		return;
	}

	position = door->position-(DOORSPEED*TicCount);	/* Close a little more */

	if (position <= 0) {		/* Will I close now? */
	/* door is closed all the way, so disconnect the areas*/
		SoundNumPtr = MapPtr->areasoundnum;	/* Unlink the sound areas */
		RemoveConnection(SoundNumPtr[door->area1],SoundNumPtr[door->area2]);
		ConnectAreas();		/* Reset areabyplayer */
		door->action = DR_CLOSED;	/* It's closed */
		position = 0;	/* Mark as closed */
	}
	door->position = position;	/* Set the new position */
}

/**********************************

	Process all the doors each game loop

**********************************/

void MoveDoors(void)
{
	Word dooron;		/* Which door am I on? */
	door_t *door;		/* Pointer to current door */

	dooron = numdoors;	/* How many doors to scan? */
	if (dooron) {		/* Any doors at all? */
		door = doors;		/* Pointer to the first door */
		do {
			switch (door->action) {	/* Call the action code */
			case DR_OPEN:
				DoorOpen(door);		/* Check to close the door */
				break;
			case DR_OPENING:		/* Continue the door opening */
				DoorOpening(door);
				break;
			case DR_CLOSING:		/* Close the door */
				DoorClosing(door);
			}
			++door;			/* Next pointer */
		} while (--dooron);	/* All doors done? */
	}
}
