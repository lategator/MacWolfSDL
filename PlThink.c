#include "WolfDef.h"

#define SHOTRATE 6

/**********************************

	Change my weapon to the biggest one I got

**********************************/

static void OutOfAmmo(void)
{
	if (gamestate.missile && gamestate.missiles) {
		gamestate.pendingweapon = WP_MISSILE;
		return;		/* Launcher & missiles */
	}
	if (gamestate.flamethrower && gamestate.gas) {
		gamestate.pendingweapon = WP_FLAMETHROWER;
		return;		/* Flames and gas */
	}
	if (gamestate.ammo) {
		if (gamestate.chaingun) {
			gamestate.pendingweapon = WP_CHAINGUN;
			return;
		}
		if (gamestate.machinegun) {
			gamestate.pendingweapon = WP_MACHINEGUN;
			return;
		}
		gamestate.pendingweapon = WP_PISTOL;
		return;
	}
	gamestate.pendingweapon = WP_KNIFE;
}


/**********************************

	Draw the ammo for the new weapon

**********************************/

void ChangeWeapon (void)
{
	switch(gamestate.weapon) {	/* Which weapon */
	case WP_PISTOL:
	case WP_MACHINEGUN:
	case WP_CHAINGUN:
		IO_DrawAmmo(gamestate.ammo);	/* Draw bullets */
		break;
	case WP_FLAMETHROWER:
		IO_DrawAmmo(gamestate.gas);		/* Draw gasoline */
		break;
	case WP_MISSILE:
		IO_DrawAmmo(gamestate.missiles);	/* Draw missiles */
		break;
	default:
		break;
	}
}

/**********************************

	Fire my weapon, change to knife if no more ammo

**********************************/

void Cmd_Fire(void)
{
	switch(gamestate.weapon) {
	case WP_CHAINGUN:
	case WP_MACHINEGUN:
	case WP_PISTOL:
		if (!gamestate.ammo) {	/* Do I have ammo? */
			OutOfAmmo();	/* Change the weapon */
		}
		break;
	case WP_FLAMETHROWER:
		if (!gamestate.gas) {
			OutOfAmmo();	/* Change the weapon */
		}
		break;
	case WP_MISSILE:
		if (!gamestate.missiles) {
			OutOfAmmo();	/* Change the weapon */
		}
		break;
	default:
		break;
	}
	gamestate.attackframe = 1;		/* Begin the attack */
	gamestate.attackcount = SHOTRATE;		/* Time before next action */
}

/**********************************

	Try to use a switch, door or pushwall...

**********************************/

void Cmd_Use(void)
{
	Word dir;		/* Direction facing */
	Word tile;		/* Tile tested */
	Word x,y;		/* x,y of the tile to test */

/* Find which cardinal direction the player is facing */

	x = actors[0].x >> FRACBITS;	/* Get the x,y in tiles */
	y = actors[0].y >> FRACBITS;

	dir = ((gamestate.viewangle+ANGLES/8)&(ANGLES-1)) >> 7;	/* Force into a cardinal direction */

	switch (dir) {
	case 0:
		x++;
		dir = CD_EAST;	/* East */
		break;
	case 1:
		y--;
		dir = CD_NORTH;	/* North */
		break;
	case 2:
		x--;
		dir = CD_WEST;	/* West */
		break;
	case 3:
		y++;
		dir = CD_SOUTH;	/* South */
	}

	tile = tilemap[y][x];		/* Get the tile data */
	if (tile & TI_DOOR) {		/* Is this a door? */
		OperateDoor(tile&TI_NUMMASK);	/* Door # to operate */
		return;
	}

	if (!PushWallRec.pwallcount && (tile&TI_PUSHWALL)) {	/* Push wall? */
		PushWall(x,y,dir);			/* Note: Only 1 pushwall at a time! */
		return;
	}

	if (tile & TI_SWITCH) {		/* Elevator switch? */
		PlaySound(SND_THROWSWITCH|0x8000);
		playstate = EX_COMPLETED;
		return;
	}
	if (tile & TI_SECRET) {		/* Secret level? */
		PlaySound(SND_THROWSWITCH|0x8000);
		playstate = EX_SECRET;
		return;
	}
	if (tile & TI_BLOCKSIGHT) {
		PlaySound(SND_HITWALL|0x8000);		/* Oof! */
	}
}

/**********************************

	Change my weapon

**********************************/

void Cmd_ChangeWeapon(void)
{
	for (;;) {
		++gamestate.pendingweapon;		/* Next weapon */
		switch(gamestate.pendingweapon) {
		case WP_PISTOL:
			if (gamestate.ammo)	{	/* Do I have ammo ? */
				return;		/* Use it! */
			}
			break;
		case WP_MACHINEGUN:
			if (gamestate.machinegun && gamestate.ammo) {
				return;		/* Machine gun & ammo! */
			}
			break;
		case WP_CHAINGUN:
			if (gamestate.chaingun && gamestate.ammo) {
				return;			/* Chain gun & ammo! */
			}
			break;
		case WP_FLAMETHROWER:
			if (gamestate.flamethrower && gamestate.gas) {
				return;		/* Flames and gas */
			}
			break;
		case WP_MISSILE:
			if (gamestate.missile && gamestate.missiles) {
				return;		/* Launcher & missiles */
			}
			break;
		default:
			gamestate.pendingweapon = WP_KNIFE;	/* Force as the knife */
			return;
		}
	}
}

/**********************************

	Sets actor to the closest enemy in the line of fire, or 0
	if there is no valid target

**********************************/

actor_t *TargetEnemy (void)
{
	Word i;
	vissprite_t	*dseg;
	actor_t *ActorPtr;

/* look at the drawn sprites from closest to farthest*/

	i = numvisspr;
	if (i) {		/* Any drawn? */
		do {
			dseg = xevents[i-1];		/* Index to the CLOSEST sprite */
			if (dseg->actornum) {	/* static sprite or missile */
				if (xscale[CENTERX] <= dseg->clipscale) {	/* Not obscured by a wall*/
					if (dseg->x1 <= CENTERX+8 && dseg->x2 >= CENTERX-8) {
						ActorPtr = &actors[dseg->actornum];		/* Get pointer to the actor */
						if (!(ActorPtr->flags & FL_DEAD) && (ActorPtr->flags & FL_SHOOTABLE)) {		/* Dead already? */
							return ActorPtr;			/* Shoot me! */
						}
					}
				}
			}
		} while (--i);
	}
	return 0;		/* No actor in range */
}

/**********************************

	Attack with a knife

**********************************/

void KnifeAttack(void)
{
	actor_t *ActorPtr;

	ActorPtr = TargetEnemy();	/* Who to hit? */
	if (ActorPtr) {				/* Got someone? */
		if (CalcDistance(ActorPtr) <= KNIFEDIST) {	/* In range? */
			DamageActor(w_rnd()&15,ActorPtr);	/* Hit him! */
			PlaySound(SND_KNIFE|0x8000);
			return;
		}
	}
	PlaySound(SND_KNIFEMISS|0x8000);	/* Swish the knife */
}

/**********************************

	Attack with a pistol, machine gun, chain gun

**********************************/

void GunAttack(void)
{
	Word Damage;
	actor_t *ActorPtr;

	if (gamestate.weapon == WP_CHAINGUN ) {
		Damage = SND_CHAIN|0x8000;		/* Bang! */
	} else if (gamestate.weapon == WP_MACHINEGUN) {
		Damage = SND_MGUN|0x8000;
	} else {
		Damage = SND_GUNSHT|0x8000;
	}
	PlaySound(Damage);			/* Play the gun shot sound */
	madenoise = TRUE;			/* I made some noise */
	if (!gamestate.godmode) {
		--gamestate.ammo;			/* Remove a bullet */
	}
	IO_DrawAmmo(gamestate.ammo);	/* Redraw the ammo */

	ActorPtr = TargetEnemy();	/* Anyone to hit? */
	if (ActorPtr) {				/* Found one? */
		if (CalcDistance(ActorPtr) < 2*TILEGLOBAL) {	/* Close range? */
			Damage = w_rnd()&15;		/* More damage */
		} else {
			Damage = w_rnd()&7;
		}
		DamageActor(Damage,ActorPtr);	/* Shoot! */
	}
}

/**********************************

	Attack with a flame thrower

**********************************/

void FlameAttack(void)
{
	int	x;		/* Sine,cos value */
	missile_t *MissilePtr;	/* Pointer to new missile record */

	madenoise = TRUE;
	PlaySound(SND_FTHROW|0x8000);	/* Burn! */
	if (!gamestate.godmode) {
		--gamestate.gas;		/* Use a gallon of gas */
	}
	IO_DrawAmmo(gamestate.gas);	/* Draw the gas value */

/* flame sprite*/

	MissilePtr = GetNewMissile();	/* Get a missile record */
	MissilePtr->areanumber = actors[0].areanumber;	/* Start where I am */
	x = costable[gamestate.viewangle];	/* Set the x velocity */
	MissilePtr->xspeed = x/5;	/* Set the speed */
	MissilePtr->x = (x/4) + actors[0].x;
	x = -sintable[gamestate.viewangle];	/* Set the y velocity */
	MissilePtr->yspeed = x/5;	/* Set the speed */
	MissilePtr->y = (x/4) + actors[0].y;
	MissilePtr->pic = S_FIREBALL;	/* Use the fireball pic */
	MissilePtr->flags = MF_HITENEMIES | MF_HITSTATICS;
	MissilePtr->type = MI_PFLAME;
}

/**********************************

	Attack with a missile

**********************************/

void MissileAttack(void)
{
	int	x;		/* Sine,Cos value */
	missile_t *MissilePtr;	/* Pointer to new missile record */

	PlaySound(SND_ROCKET|0x8000);	/* Fire! */
	madenoise = TRUE;		/* Made a racket */
	if (!gamestate.godmode) {
		--gamestate.missiles;
	}
	IO_DrawAmmo(gamestate.missiles);	/* Draw the missile count */
	MissilePtr = GetNewMissile();	/* missile sprite */
	MissilePtr->areanumber = actors[0].areanumber;
	x = costable[gamestate.viewangle];
	MissilePtr->x = (x/4) + actors[0].x;	/* Must be visible for screen! */
	MissilePtr->xspeed = (x/5);
	x = -sintable[gamestate.viewangle];
	MissilePtr->y = (x/4) + actors[0].y;
	MissilePtr->yspeed = (x/5);
	MissilePtr->pic = S_MISSILE;
	MissilePtr->flags = MF_HITENEMIES | MF_HITSTATICS;
	MissilePtr->type = MI_PMISSILE;
}

/**********************************

	Move the player (Called every tic)

**********************************/

void MovePlayer(void)
{
	ControlMovement();		/* Read the controller */

/* check for use*/

	if (buttonstate[bt_use]) {	/* Pressed use? */
		if (!useheld) {
			useheld = TRUE;		/* Only once... */
			Cmd_Use();
		}
	} else {
		useheld = FALSE;	/* It's released */
	}

/* check for weapon select */

	if (buttonstate[bt_select]) {
		if (!selectheld) {
			selectheld = TRUE;	/* Only once */
			Cmd_ChangeWeapon();	/* Change the weapon */
		}
	} else {
		selectheld = FALSE;		/* It's released */
	}

/* check for starting an attack */

	if (buttonstate[bt_attack]) {
		if (!attackheld && !gamestate.attackframe) {
			attackheld = TRUE;	/* Held down */
			Cmd_Fire();	/* Initiate firing my weapon */
		}
	} else {
		attackheld = FALSE;	/* It's released */
	}

/* advance attacking action */

	if (!gamestate.attackframe) {	/* Not in the middle of firing the weapon? */
		if (gamestate.pendingweapon != gamestate.weapon) {	/* New weapon? */
			gamestate.weapon = gamestate.pendingweapon;	/* Set the weapon */
			ChangeWeapon();		/* Redraw the ammo */
		}
		return;	/* Exit now */
	}

/* I am in an attack, time yet? */

	if (gamestate.attackcount>TicCount) {	/* Wait to attack! */
		gamestate.attackcount-=TicCount;	/* Wait a bit */
		return;
	}

/* change frame */

	if (TicCount<SHOTRATE) {
		gamestate.attackcount += (SHOTRATE-TicCount);		/* Carry over the time */
	}
	++gamestate.attackframe;		/* Next animation frame */

/* frame 2: attack frame */

	if (gamestate.attackframe == 2) {
		switch (gamestate.weapon) {
		case WP_KNIFE:
			KnifeAttack();		/* Knife 'em */
			break;
		case WP_PISTOL:
			if (!gamestate.ammo) {
				gamestate.attackframe = 4;
				return;
			}
			GunAttack();		/* One shot */
			break;
		case WP_MACHINEGUN:
			if (!gamestate.ammo) {
				gamestate.attackframe = 4;
				return;
			}
			GunAttack();		/* First shot */
			break;
		case WP_CHAINGUN:
			if (!gamestate.ammo) {
				gamestate.attackframe = 4;
				return;
			}
			GunAttack();		/* First of many shots */
			break;
		case WP_FLAMETHROWER:
			if (!gamestate.gas) {
				gamestate.attackframe = 4;
				return;
			}
			FlameAttack();	/* Shoot a flame */
			break;
		case WP_MISSILE:
			if (!gamestate.missiles) {
				gamestate.attackframe = 4;
				return;
			}
			MissileAttack();	/* Shoot the missile */
			break;
		default:
			break;
		}
	}

/* frame 3: second shot for chaingun and flamethrower */

	if (gamestate.attackframe == 3) {
		if (gamestate.weapon == WP_CHAINGUN ) {
			if (!gamestate.ammo) {
				++gamestate.attackframe;
			} else {
				GunAttack();		/* Shoot! */
			}
		} else if (gamestate.weapon == WP_FLAMETHROWER )  {
			if (!gamestate.gas) {
				++gamestate.attackframe;
			} else {
				FlameAttack();		/* Flame on! */
			}
		}
		return;
	}

/* frame 4: possible auto fire */

	if (gamestate.attackframe == 4) {
		if (buttonstate[bt_attack]) {	/* Held down? */
			if ( (gamestate.weapon == WP_MACHINEGUN
			|| gamestate.weapon == WP_CHAINGUN) && gamestate.ammo) {
				gamestate.attackframe = 2;	/* Fire again */
				GunAttack();				/* Shoot */
			}
			if (gamestate.weapon == WP_FLAMETHROWER && gamestate.gas) {
				gamestate.attackframe=2;
				FlameAttack();		/* Flame again */
			}
		}
		return;
	}

	if (gamestate.attackframe == 5) {		/* End the attack */
		switch (gamestate.weapon) {
		case WP_CHAINGUN:
		case WP_MACHINEGUN:
		case WP_PISTOL:
			if (!gamestate.ammo) {
				OutOfAmmo();		/* Switch weapons */
			} else if (gamestate.weapon == WP_PISTOL && buttonstate[bt_attack]
			&& !attackheld) {
				attackheld = TRUE;
				gamestate.attackframe = 1;	/* Fire again the pistol */
				return;
			}
			break;
		case WP_FLAMETHROWER:
			if (!gamestate.gas) {		/* Out of gas? */
				OutOfAmmo();		/* Change weapons */
			}
			break;
		case WP_MISSILE:
			if (!gamestate.missiles) {	/* Out of missiles? */
				OutOfAmmo();		/* Switch weapons */
			}
			break;
		default:
			break;
		}
		gamestate.attackcount = 0;		/* Shut down the attack */
		gamestate.attackframe = 0;
	}
}
