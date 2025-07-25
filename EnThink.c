#include "WolfDef.h"

/**********************************

	Drops a bonus item at the x,y of the actor
	If there are no free item spots, nothing is done.

**********************************/

void PlaceItemType(Word shape,actor_t *ActorPtr)
{
	Word tile;
	Word x,y;
	static_t *StaticPtr;

	if (numstatics>=MAXSTATICS) {	/* Already full? */
		return;						/* Get out! */
	}
	StaticPtr = &statics[numstatics];	/* Get pointer to the record */

/* drop bonus items on closest tile, rather than goal tile (unless it is a closing door) */

	x = ActorPtr->x >> FRACBITS;
	y = ActorPtr->y >> FRACBITS;
	tile = tilemap[y][x];
	if ( (tile&TI_BLOCKMOVE) && !(tile &TI_ACTOR) ) {
		x = ActorPtr->goalx;
		y = ActorPtr->goaly;
	}
	StaticPtr->pic = shape;
	StaticPtr->x = (x<<FRACBITS)|0x80;
	StaticPtr->y = (y<<FRACBITS)|0x80;
	StaticPtr->areanumber = ActorPtr->areanumber;
	tilemap[y][x] |= TI_GETABLE;		/* Mark as getable */
	++numstatics;		/* A new static */
}

/**********************************

	Kill an actor
	Also drop off any items you can get from a dead guy.

**********************************/

void KillActor(actor_t *ActorPtr)
{
	Word x,y;

	GivePoints(classinfo[ActorPtr->class].points);	/* Award the score */
	switch(ActorPtr->class) {	/* Drop anything special? */
	case CL_SS:
		PlaceItemType(S_MACHINEGUN,ActorPtr);	/* Give a gun */
		break;
	case CL_OFFICER:
	case CL_MUTANT:
	case CL_GUARD:
		PlaceItemType(S_AMMO,ActorPtr);	/* Drop some ammo */
		break;
	case CL_HANS:
	case CL_SCHABBS:
	case CL_TRANS:
	case CL_UBER:
		PlaceItemType(S_G_KEY,ActorPtr);	/* Drop a key */
		break;
	case CL_DKNIGHT:
		PlaceItemType(S_S_KEY,ActorPtr);	/* Drop a key */
		break;
	default:
		break;
	}
	++gamestate.killcount;		/* I killed someone! */
	ActorPtr->flags = FL_DEAD;	/* remove old actor marker*/
	tilemap[ActorPtr->goaly][ActorPtr->goalx] &= ~TI_ACTOR;
	x = ActorPtr->x >> FRACBITS;
	y = ActorPtr->y >> FRACBITS;
	tilemap[y][x] |= TI_BODY;	/* body flag on most apparant, no matter what */
	NewState(ActorPtr,classinfo[ActorPtr->class].deathstate);	/* start the death animation */
}

/**********************************

	Does damage points to enemy actor, either putting it into a stun frame or
	killing it.
	Called when an enemy is hit.

**********************************/

static Word PainTick;
void DamageActor(Word damage,actor_t *ActorPtr)
{
	stateindex_t pain;

	madenoise = TRUE;	/* You made some noise! */

/* do double damage if shooting a non attack mode actor*/

	if ( !(ActorPtr->flags & FL_ACTIVE) ) {
		if (difficulty<3) {		/* Death incarnate? */
			damage <<= 1;
		}
		FirstSighting(ActorPtr);			/* Put into combat mode*/
	}

	if (damage >= ActorPtr->hitpoints) {	/* Did I kill it? */
		KillActor(ActorPtr);				/* Die!! */
		return;
	}

	ActorPtr->hitpoints -= damage;		/* Remove the damage */
	if (ActorPtr->class == CL_MECHAHITLER && ActorPtr->hitpoints <= 250 && ActorPtr->hitpoints+damage > 250) {
	/* hitler losing armor */
		PlaySound(SND_SHIT);	/* Remove armor */
		pain = ST_MHITLER_DIE1;
	} else {
		if ((ReadTick() - PainTick) >= 30) {
			PainTick = ReadTick();
			PlaySound(SND_PAIN);		/* Ow!! */
		}
		pain = classinfo[ActorPtr->class].painstate;	/* Do pain */
	}
	if (pain) {	/* some classes don't have pain frames */
		if (ActorPtr->state != pain) {	/* Already in pain? */
			NewState(ActorPtr,pain);
		}
	}
}

/**********************************

	Throw a Missile at the player

**********************************/

void A_Throw(actor_t *ActorPtr)
{
	Word angle;
	int	speed;
	missile_t *MissilePtr;

	PlaySound(SND_ROCKET|0x8000);
	MissilePtr = GetNewMissile();		/* Create a missile */
	MissilePtr->x = ActorPtr->x;
	MissilePtr->y = ActorPtr->y;
	MissilePtr->areanumber = ActorPtr->areanumber;

	/* get direction from enemy to player */
	angle = PointToAngle(ActorPtr->x,ActorPtr->y);
	angle >>= SHORTTOANGLESHIFT;
	speed = costable[angle];
	speed = speed/5;
	MissilePtr->xspeed = -speed;
	speed = sintable[angle];
	speed = speed/5;
	MissilePtr->yspeed = speed;
	MissilePtr->pic = S_NEEDLE;		/* Hurl a needle */
	MissilePtr->flags = MF_HITPLAYER | MF_HITSTATICS;	/* Can hit the player */
	MissilePtr->type = MI_NEEDLE;	/* Needle missile */
}

/**********************************

	Launch a rocket at the player

**********************************/

void A_Launch(actor_t *ActorPtr)
{
	Word angle;
	int	speed;
	missile_t *MissilePtr;

	PlaySound(SND_ROCKET|0x8000);
	MissilePtr = GetNewMissile();
	MissilePtr->x = ActorPtr->x;
	MissilePtr->y = ActorPtr->y;
	MissilePtr->areanumber = ActorPtr->areanumber;

	/* get direction from player to boss*/
	angle = PointToAngle (ActorPtr->x,ActorPtr->y);
	angle >>= SHORTTOANGLESHIFT;
	speed = costable[angle];
	speed = speed/5;
	MissilePtr->xspeed = -speed;
	speed = sintable[angle];
	speed = speed/5;
	MissilePtr->yspeed = speed;
	MissilePtr->pic = S_ENMISSILE;	/* Rocket */
	MissilePtr->flags = MF_HITPLAYER | MF_HITSTATICS;
	MissilePtr->type = MI_EMISSILE;
	A_Shoot(ActorPtr);	/* also shoot a bullet */
}

/**********************************

	Scream a death sound

**********************************/

void A_Scream(actor_t *ActorPtr)
{
	Word Sound,i;

	Sound = classinfo[ActorPtr->class].deathsound;	/* Get the sound # */
	if (Sound==SND_EDIE) {		/* Normal death sound? */
		if (w_rnd()&1) {		/* Play one randomly */
			++Sound;
		}
		i = 0;
		do {
			StopSound(NaziSound[i]);	/* Kill all Nazi voices */
		} while (++i<4);
	}
	PlaySound(Sound);		/* Play the sound */
}

/**********************************

	Body hitting the ground

**********************************/

void A_Thud(actor_t *ActorPtr)
{
	PlaySound(SND_BODYFALL);
}

/**********************************

	You win the game!

**********************************/

void A_Victory(actor_t *ActorPtr)
{
	playstate = EX_COMPLETED;
}

/**********************************

	Drop Hitler's armor and let hitler run around

**********************************/

void A_HitlerMorph(actor_t *ActorPtr)
{
	missile_t *MissilePtr;

/* Use an inert missile for the armor remnants */

	MissilePtr = GetNewMissile();
	MissilePtr->x = ActorPtr->x;	/* Pass the armor x,y */
	MissilePtr->y = ActorPtr->y;
	MissilePtr->areanumber = ActorPtr->areanumber;
	MissilePtr->xspeed = 0;	/* No motion */
	MissilePtr->yspeed = 0;
	MissilePtr->flags = 0;
	MissilePtr->type = -1;			/* Maximum time */
	MissilePtr->pic = S_MHITLER_DIE4;	/* Set the picture */
	ActorPtr->class = CL_HITLER;	/* Convert to true hitler */
	ActorPtr->speed = 40/4;		/* faster without armor*/
}

/**********************************

	Try to damage the player, based on skill level and player's speed

**********************************/

void A_Shoot(actor_t *ActorPtr)
{
	Word damage;		/* Damage to inflict */
	Word distance;

	if (!areabyplayer[MapPtr->areasoundnum[ActorPtr->areanumber]]) {	/* In the same area? */
		return;
	}

	madenoise = TRUE;	/* I made a sound! */
	if (ActorPtr->class >= CL_HANS) {		/* Boss? */
		PlaySound(SND_BIGGUN|0x8000);	/* Boom! */
	} else {
		PlaySound(SND_GUNSHT|0x8000);	/* Bang! */
	}

	if (!CheckLine(ActorPtr)) {	/* Player is behind a wall*/
		return;			/* Can't shoot! */
	}
	distance = CalcDistance(ActorPtr);	/* How far? (0-4095 range) */

	if (distance >= TILEGLOBAL*16) {	/* Too far away? */
		return;
	}

	if (ActorPtr->class == CL_OFFICER || ActorPtr->class >= CL_HANS) {	/* better shots */
		if (distance < (16*16)) {
			distance = 0;		/* Zap the distance */
		} else {
			distance -= (16*16);
		}
	}

	if (playermoving) {	/* harder to hit when moving*/
		if (distance >= (224*16)) {
			return;
		}
		distance += (32*16);
	}

/* see if the shot was a hit*/

	if ((w_rnd()*16)>distance) {
		switch(difficulty) {
		case 0:
			damage = (w_rnd()&3)+1;
			break;
		case 1:
			damage = (w_rnd()&7)+1;
			break;
		default:
			damage = (w_rnd()&7)+3;
		}
		if (distance<(32*16)) {
			damage <<= 2;
		} else if (distance<(64*16)) {
			damage <<= 1;
		}
		TakeDamage(damage,ActorPtr->x,ActorPtr->y);	/* Hit the player (Pass the killer's x,y) */
	}
}

/**********************************

	Bite the player

**********************************/

void A_Bite(actor_t *ActorPtr)
{
	Word dmg;

	PlaySound(SND_DOGBARK);	/* Take a bite! */
	if (CalcDistance(ActorPtr)<=BITERANGE) {	/* In range? */
		switch (difficulty) {
		case 0:
			dmg = (w_rnd()&3)+3;	/* Small bite */
			break;
		case 1:
			dmg = (w_rnd()&7)+3;	/* Medium bite */
			break;
		default:
			dmg = (w_rnd()&15)+4;	/* BIG bite */
		}
		TakeDamage(dmg,ActorPtr->x,ActorPtr->y);	/* Pass along the damage */
	}
}

void A_Drain(actor_t *ActorPtr)
{
	if (CalcDistance(ActorPtr)<=BITERANGE) {	/* In range? */
		TakeDamage(15,ActorPtr->x,ActorPtr->y);	/* Pass along the damage */
	}
}

/**********************************

	Return the distance between the player and this actor

**********************************/

Word CalcDistance(actor_t *ActorPtr)
{
	Word absdx;
	Word absdy;

	absdx = w_abs(ActorPtr->x - actors[0].x);
	absdy = w_abs(ActorPtr->y - actors[0].y);
	return (absdx > absdy) ? absdx : absdy;	/* Return the larger */
}

/**********************************

	Called every few frames to check for sighting and attacking the player

**********************************/

Word shootchance[8] = {256,64,32,24,20,16,12,8};

void A_Target(actor_t *ActorPtr)
{
	Word chance;	/* % chance of hit */
	Word distance;	/* Distance of critters */

	if (!areabyplayer[MapPtr->areasoundnum[ActorPtr->areanumber]] || !CheckLine(ActorPtr)) {
		ActorPtr->flags &= ~FL_SEEPLAYER;	/* Can't see you */
		return;
	}

	ActorPtr->flags |= FL_SEEPLAYER;		/* I see you */
	distance = CalcDistance(ActorPtr);		/* Get the distance */

	if (distance < BITERANGE) {	/* always attack when this close */
		goto attack;
	}

	if (ActorPtr->class == CL_DOG) {	/* Dogs can only bite */
		return;
	}

	if (ActorPtr->class == CL_SCHABBS && distance <= TILEGLOBAL*4) {
		goto attack;		/* Dr. schabbs always attacks */
	}

	if (distance >= TILEGLOBAL*8) {		/* Too far? */
		return;
	}

	chance = shootchance[distance>>FRACBITS];	/* Get the base chance */
	if (difficulty >= 2) {
		chance <<= 1;		/* Increase chance */
	}
	if (w_rnd() < chance) {
attack:		/* go into attack frame*/
		NewState(ActorPtr,classinfo[ActorPtr->class].attackstate);
	}
}

/**********************************

	MechaHitler takes a step

**********************************/

void A_MechStep(actor_t *ActorPtr)
{
	PlaySound(SND_MECHSTEP|0x8000);	/* Step sound */
	A_Target(ActorPtr);	/* Shoot player */
}

/**********************************

	Chase the player

**********************************/

void T_Chase(actor_t *ActorPtr)
{
	Word move;

/* if still centered in a tile, try to find a move */

	if (ActorPtr->flags & FL_NOTMOVING) {
		if (ActorPtr->flags & FL_WAITDOOR) {
			TryWalk(ActorPtr);		/* Waiting for a door to open*/
		} else if (ActorPtr->flags & FL_SEEPLAYER) {
			SelectDodgeDir(ActorPtr);	/* Dodge the player's bullets */
		} else {
			SelectChaseDir(ActorPtr);	/* Directly chase the player */
		}
		if (ActorPtr->flags & FL_NOTMOVING) {
			return;			/* Still blocked in */
		}
	}

/* OPTIMIZE: integral steps / tile movement */

/* cover some distance*/

	move = ActorPtr->speed*TicCount;		/* this could be put in the class info array*/

	while (move) {
		if (move < ActorPtr->distance) {
			MoveActor(ActorPtr,move);	/* Move one step */
			return;
		}

		/* reached goal tile, so select another one*/

		move -= ActorPtr->distance;
		MoveActor(ActorPtr,ActorPtr->distance);		/* move the last 1 to center*/

		if (ActorPtr->flags & FL_SEEPLAYER) {
			SelectDodgeDir(ActorPtr);	/* Dodge the player */
		} else {
			SelectChaseDir(ActorPtr);	/* Directly chase the player */
		}
		if (ActorPtr->flags & FL_NOTMOVING) {
			return;						/* object is blocked in*/
		}
	}
}

/**********************************

	Move all actors for a single frame
	Actions are performed as the state is entered

**********************************/

typedef void (*call_t)(actor_t *ActorPtr);

static void A_Nothing(actor_t *ActorPtr) {}

call_t thinkcalls[] = {
	A_Nothing,	/* No action */
	T_Stand,	/* Stand at attention */
	T_Chase		/* Chase the player */
};


call_t actioncalls[] = {
	A_Nothing,
	A_Target,
	A_Shoot,
	A_Bite,
	A_Throw,
	A_Launch,
	A_HitlerMorph,
	A_MechStep,
	A_Victory,
	A_Scream,
	A_Thud,
	A_Drain,
};

void MoveActors(void)
{
	Word i;		/* Index */
	state_t *StatePtr;	/* Pointer to state logic */
	actor_t *ActorPtr;	/* Pointer to Actor code */

	if (numactors<2) {	/* No actors to check? */
		return;
	}
	i = 1;					/* Init index */
	ActorPtr = &actors[1];	/* Init the pointer to the actors */
	do {
		if (!(ActorPtr->flags&FL_ACTIVE)	/* Is this actor in view? */
		&& !areabyplayer[MapPtr->areasoundnum[ActorPtr->areanumber]]) {
			goto Skip;
		}

		StatePtr = &states[ActorPtr->state];	/* Get the current state */
		if (ActorPtr->ticcount>TicCount) {	/* Count down the time */
			ActorPtr->ticcount-=TicCount;
		} else {		/* change state if time's up */
			ActorPtr->state = StatePtr->next;	/* Set the next state */
			StatePtr = &states[ActorPtr->state];	/* Get the new state ptr */
			ActorPtr->ticcount = StatePtr->tictime;	/* Reset the time */
			ActorPtr->pic = StatePtr->shapenum;		/* Set the new picture # */
			/* action think */
			actioncalls[StatePtr->action](ActorPtr);	/* Call the code */
		}
		thinkcalls[StatePtr->think](ActorPtr);	/* Perform the action */
Skip:	/* Next entry */
		++ActorPtr;
	} while (++i<numactors);
}
