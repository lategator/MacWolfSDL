#include "WolfDef.h"

/**********************************

	Returns True if a straight line between the player and actor is unobstructed

**********************************/

Boolean CheckLine(actor_t *ActorPtr)
{
	Word actorx,actory,actortx,actorty;		/* Current actor's coords */
	Word playerx,playery,playertx,playerty;	/* Player's coords */
	Word xl,xh,yl,yh;				/* Min x,y Max x,y for test */
	int delta;		/* Pixel differance to calc Step and Frac */
	int Step;		/* Step value for each whole xy */
	Word Frac;		/* Fractional xy stepper */
	Word deltafrac;	/* True distance for whole numbers */
	Word intercept;	/* Temp for door code */
	Word tile;		/* Temp for tile check */
	Byte partial;	/* Fraction to force whole numbers */

	actorx = ActorPtr->x;		/* Get the actor's x,y */
	actortx = actorx>>FRACBITS;	/* Get the actor's tile x,y */
	actory = ActorPtr->y;
	actorty = actory>>FRACBITS;

	playerx = actors[0].x;	/* Get the player's x,y */
	playertx = playerx>>FRACBITS;	/* Get the player's tile x,y */
	playery = actors[0].y;
	playerty = playery>>FRACBITS;

	/* The actor COULD be standing on a blocked tile (On a CLOSING door tile) */

#if 0
	if (tilemap[actorty][actortx] & TI_BLOCKSIGHT) {	/* Is the actor IN a wall? */
		return FALSE;			/* This could happen on a closing door */
	}
#endif

/* check for solid tiles at x crossings */

	if (playertx!=actortx) {		/* Scan in the x direction? */
		if (actortx<playertx) {
			partial = -actorx;	/* Isolate the fraction */
			xl = actortx-1;			/* Actor is on the left side */
			xh = playertx-1;
			yl = actory;
			yh = playery;
			deltafrac = playerx-actorx;	/* Distance in pixels */
		} else {
			partial = -playerx;
			xl = playertx;			/* Player is on the left side */
			xh = actortx;
			yl = playery;
			yh = actory;
			deltafrac = actorx-playerx;	/* Distance in pixels */
		}
		delta = yh-yl;				/* Y adjust (Signed) */
		if (w_abs(delta) >= (16*FRACUNIT) || deltafrac >= (16*FRACUNIT)) {	/* Farther than 16 tiles? */
			return FALSE;
		}
		Step = FixedDiv(delta,deltafrac);		/* How much to y step */
		Frac = FixedByFrac(Step,partial)+yl;	/* Init the Y coord */
		do {
			++xl;
			tile = tilemap[Frac>>FRACBITS][xl];		/* Get the current tile */
			if (tile & TI_BLOCKSIGHT) {
				return FALSE;		/* Can't see! */
			}
			if (tile & TI_DOOR) {	/* see if the door is open enough*/
				intercept = ((Step/2)+Frac)&0xff;
				if (intercept > doors[tile&TI_NUMMASK].position) {
					return FALSE;	/* Can't see! */
				}
			}
			Frac += Step;
		} while (xl<xh);
	}

/* check for solid tiles at y crossings */

	if (playerty!=actorty) {
		if (actorty<playerty) {
			partial = -actory;
			xl = actorx;
			xh = playerx;
			yl = actorty-1;
			yh = playerty-1;
			deltafrac = playery-actory;
		} else {
			partial = -playery;
			xl = playerx;
			xh = actorx;
			yl = playerty;
			yh = actorty;
			deltafrac = actory-playery;
		}
		delta = xh-xl;		/* Number of tiles to scan */
		if (w_abs(delta) >= 16*FRACUNIT || deltafrac >= 16*FRACUNIT) {
			return FALSE;
		}
		Step = FixedDiv(delta,deltafrac);
		Frac = FixedByFrac(Step,partial)+xl;
		do {
			++yl;
			tile = tilemap[yl][Frac>>FRACBITS];
			if (tile & TI_BLOCKSIGHT) {
				return FALSE;
			}
			if (tile & TI_DOOR) {	/* see if the door is open enough*/
				intercept = ((Step/2)+Frac)&0xff;
				if (intercept > doors[tile&TI_NUMMASK].position) {
					return FALSE;	/* Can't see! */
				}
			}
			Frac += Step;
		} while (yl<yh);
	}
	return TRUE;	/* You are visible */
}

/**********************************

	Puts actor into attack mode, either after reaction time or being shot

**********************************/

void FirstSighting(actor_t *ActorPtr)
{
	classinfo_t	*info;
	Word sound;

	info = &classinfo[ActorPtr->class];	/* Get pointer to info record */
	sound = info->sightsound;			/* Get the requested sound */
	if (sound == SND_ESEE) {	/* make random human sound*/
		sound = NaziSound[w_rnd()&3];	/* Make a differant sound */
	}
	PlaySound(sound|0x8000);			/* Play the sight sound */
	NewState(ActorPtr,info->sightstate);	/* Set the next state */
	ActorPtr->flags |= FL_ACTIVE;			/* Make it active */
}

/**********************************

	Called by actors that ARE NOT chasing the player. If the player
 	is detected (by sight, noise, or proximity), the actor is put into
 	it's combat frame.

 	Incorporates a random reaction delay.

**********************************/

void T_Stand(actor_t *ActorPtr)
{
	if (ActorPtr->reacttime) {		/* Waiting to react? */
		if (ActorPtr->reacttime>TicCount) {	/* Still waiting? */
			ActorPtr->reacttime-=TicCount;	/* Count down */
			return;
		}
		if (ActorPtr->flags&FL_AMBUSH ) {	/* Ambush actor? */
			if (!CheckLine(ActorPtr)) {	/* Can I see you? */
				ActorPtr->reacttime=1;	/* be very ready, but*/
				return;					/* don't attack yet*/
			}
			ActorPtr->flags &= ~FL_AMBUSH;	/* Clear the ambush flag */
		}
		FirstSighting(ActorPtr);		/* Attack the player! */
		ActorPtr->reacttime=0;
	}

/* Haven't seen player yet*/

	if (madenoise || CheckLine(ActorPtr)) {	/* Made a gun shot? Seen? */
		ActorPtr->reacttime = (w_rnd() & classinfo[ActorPtr->class].reactionmask)*4+1;
	}
}