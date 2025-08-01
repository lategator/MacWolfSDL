#include "WolfDef.h"

/**********************************

	Returns a pointer to an empty missile record

**********************************/

missile_t *GetNewMissile(void)
{
	Word Count;

	Count = nummissiles;
	if (Count < MAXMISSILES) {	/* Already full? */
		++Count;					/* Next entry */
		nummissiles = Count;		/* Save the new missile count */
	}
	return &missiles[Count-1];	/* Get pointer to empty missile */
}

/**********************************

	Explode a missile

**********************************/

void ExplodeMissile(missile_t *MissilePtr)
{
	MissilePtr->flags = 0;		/* Can't harm anyone! */
	if (MissilePtr->type == MI_PMISSILE) {
		MissilePtr->pic = S_MISSBOOM;	/* Rocket explosion */
		MissilePtr->type = 16;		/* Tics to stay in explosion*/
	} else {
		MissilePtr->pic = S_FIREBOOM;	/* Fire explosion */
		MissilePtr->type = 8;		/* Tics to stay in explosion*/
	}
	PlaySound(SND_BOOM|0x8000);			/* BOOM! */
	MissilePtr->x -= MissilePtr->xspeed;	/* Undo the last motion */
	MissilePtr->y -= MissilePtr->yspeed;
	MissilePtr->xspeed = 0;			/* Don't move anymore */
	MissilePtr->yspeed = 0;
}

/**********************************

	Take damage from the player
	Damage is 8,16...64, pass the x,y for a kill x,y

**********************************/

void MissileHitPlayer(missile_t *MissilePtr)
{
	TakeDamage((w_rnd()&0x38)+8,MissilePtr->x,MissilePtr->y);	/* Inflict damage */
}

/**********************************

	Take damage from the enemy
	Detonate if not killed (return True)

**********************************/

Boolean MissileHitEnemy(missile_t *MissilePtr, actor_t *ActorPtr)
{
	Word Damage;
	if (MissilePtr->type == MI_PMISSILE) {	/* Rocket? */
		Damage = (w_rnd()&0x30)+16;	/* Hit'm hard! */
	} else {
		Damage = (w_rnd()&0x0f)+1;	/* Burn a little */
	}
	DamageActor(Damage,ActorPtr);	/* Hit'em! */
	return !(ActorPtr->flags & FL_DEAD); 	/* Not dead? */
}

/**********************************

	Did the missile impact on an actor

**********************************/

Boolean CheckMissileActorHits(missile_t *MissilePtr)
{
	Word xl,xh,yh;
	Word x,y;
	Word Bang;
	Word tile;
	actor_t *ActorPtr;

	Bang = 0;			/* Assume the missile is OK */
	xl = (MissilePtr->x>>FRACBITS)-1;	/* Create the kill rect */
	xh = xl+3;
	y = (MissilePtr->y>>FRACBITS)-1;
	yh = y+3;
	if (xl>=MAPSIZE) {		/* Clip the attack rect to STAY on the map! */
		xl = 0;
	}
	if (y>=MAPSIZE) {
		y = 0;
	}
	if (xh>=MAPSIZE) {
		xh = MAPSIZE;
	}
	if (yh>=MAPSIZE) {
		yh = MAPSIZE;
	}
	do {
		x = xl;		/* Begin the X scan */
		do {
			tile = tilemap[y][x];	/* Actor here? */
			if (tile&TI_ACTOR) {
				ActorPtr = &actors[MapPtr->tilemap[y][x]];	/* Check for impact */
				if ( (w_abs(ActorPtr->x - MissilePtr->x) < MISSILEHITDIST) &&
					(w_abs(ActorPtr->y - MissilePtr->y) < MISSILEHITDIST)) {
					Bang |= MissileHitEnemy(MissilePtr,ActorPtr); /* Detonate? */
				}
			}
		} while (++x<xh);	/* Scan the x's */
	} while (++y<yh);		/* Scan the y's */
	return Bang;	/* Return detonation value */
}

/**********************************

	Move the missiles each game frame

**********************************/

void MoveMissiles(void)
{
	Word Count;		/* Current missile # */
	missile_t *MissilePtr;	/* Pointer to missile record */
	Word i,x,y,tile;		/* Tile position */

	Count = nummissiles;	/* How many active missiles? */
	if (!Count || !TicCount) {		/* No missiles? (Or elapsed time?) */
		return;				/* Exit now */
	}

	MissilePtr = &missiles[0];	/* Init pointer to the first missile */
	do {
		if (!MissilePtr->flags) {	/* inert explosion*/
			if (MissilePtr->type>TicCount) {	/* Time up? */
				MissilePtr->type-=TicCount;		/* Remove some time */
				++MissilePtr;			/* Go to the next entry */
			} else {
				--nummissiles;		/* Remove this missile */
				MissilePtr[0] = missiles[nummissiles];	/* Copy the FINAL record here */
			}
			continue;		/* Next! */
		}

	/* move position*/

		i = TicCount;		/* How many times to try this? */
		do {
			MissilePtr->x += MissilePtr->xspeed;
			MissilePtr->y += MissilePtr->yspeed;

	/* check for contact with player*/

			if (MissilePtr->flags & MF_HITPLAYER) {
				if (w_abs(MissilePtr->x - actors[0].x) < MISSILEHITDIST
				&& w_abs(MissilePtr->y - actors[0].y) < MISSILEHITDIST) {
					MissileHitPlayer(MissilePtr);		/* Take damage */
					goto BOOM;			/* Boom! */
				}
			}

	/* check for contact with actors*/

			if (MissilePtr->flags & MF_HITENEMIES) {
				if (CheckMissileActorHits(MissilePtr)) {	/* Did it detonate? */
					goto BOOM;				/* Boom! */
				}
			}

	/* check for contact with walls and get new area */

			x = MissilePtr->x >> FRACBITS;
			y = MissilePtr->y >> FRACBITS;
			tile = tilemap[y][x];
			if (tile & TI_BLOCKMOVE) {		/* Can be a solid static or a wall*/
BOOM:
				ExplodeMissile(MissilePtr);	/* Detonate a missile */
				break;
			}
			if (!(tile&TI_DOOR) ) { /* doorways don't have real area numbers*/
				MissilePtr->areanumber = tile&TI_NUMMASK;
			}
		} while (--i);
		++MissilePtr;		/* Pointer to the next entry */
	} while (--Count);	/* No more entries? */
}