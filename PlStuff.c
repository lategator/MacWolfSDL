#include "WolfDef.h"

/**********************************

	Do damage to the player and show the killer
	if you die.

**********************************/

void TakeDamage(Word points,Word x,Word y)
{
	int angle;

	if (gamestate.godmode) {					/* Don't do anything else if a god */
		return;
	}

	if (gamestate.health <= points) {	/* Died? */
		playstate = EX_DIED;		/* Kill off the player */
		gamestate.health = 0;		/* No health */
		killx = x;					/* Mark the x,y of the killer */
		killy = y;
	} else {
		PlaySound((w_rnd()&1)+SND_OUCH1);			/* Ouch! */
		gamestate.health -= points;	/* Remove the hit points */
	}

	IO_DrawHealth(gamestate.health);	/* Draw the health */

/* change face to look at attacker */

	angle = PointToAngle(x,y) - (gamestate.viewangle<<SHORTTOANGLESHIFT);
	if (angle > 0) {
		if (angle > 0x2000) {
			faceframe = 3;
		} else {
			faceframe = 1;
		}
	} else {
		if (angle < -0x2000) {
			faceframe = 2;
		} else {
			faceframe = 0;
		}
	}
	if (gamestate.health <= 25) {	/* Feel'n pretty bad? */
		faceframe += 5;			/* use beat up frames*/
	}
	IO_DrawFace(faceframe);		/* Draw the face */
	facecount = 120;				/* Hold for 2 seconds */
}

/**********************************

	Heal the player

**********************************/

void HealSelf(Word points)
{
	gamestate.health += points;		/* Add in the health */
	if (gamestate.health>100) {
		gamestate.health = 100;	/* Maximum */
	}
	IO_DrawHealth(gamestate.health);	/* Draw the health */
	if (gamestate.health <= 25) {	/* Feel'n good? */
		faceframe = 5;			/* Use beat up frames*/
	} else {
		faceframe = 0;
	}
	IO_DrawFace(faceframe);		/* Redraw the face */
}

/**********************************

	Award a free life

**********************************/

void GiveExtraMan(void)
{
	if (gamestate.lives<10) {		/* Too many already? */
		++gamestate.lives;			/* +1 life */
		IO_DrawLives(gamestate.lives);	/* Redraw the lives */
	}
	PlaySound(SND_EXTRA);				/* Play a sound */
}

/**********************************

	Award some score to the player and give
	free lives if enough points have been accumulated

**********************************/

void GivePoints(LongWord points)
{
	gamestate.score += points;	/* Add to the score */
	while (gamestate.score >= gamestate.nextextra) {
		gamestate.nextextra += EXTRAPOINTS;	/* New free man score */
		GiveExtraMan();			/* Give a free man */
	}
	IO_DrawScore(gamestate.score);	/* Draw the new score */
}

/**********************************

	Award some score to the player and give
	free lives if enough points have been accumulated

**********************************/

void GiveTreasure(void)
{
	++gamestate.treasure;		/* Add the value of the treasure */
	while (gamestate.treasure >= 50) {
		gamestate.treasure -= 50;	/* Give a free man for 50 treasures */
		GiveExtraMan();
	}
	IO_DrawTreasure(gamestate.treasure);	/* Draw the treasure amount */
}

/**********************************

	Award a new weapon, use it if better than what
	the player already has.

**********************************/

void GiveWeapon(weapontype weapon)
{
	if (gamestate.pendingweapon < weapon) {	/* Better? */
		gamestate.pendingweapon = weapon;	/* Use it! */
	}
}

/**********************************

	Award some ammo, rearm weapon if knife was the current weapon

**********************************/

void GiveAmmo(Word ammo)
{
	gamestate.ammo += ammo;		/* Add the ammo */
	if (gamestate.ammo > gamestate.maxammo) {
		gamestate.ammo = gamestate.maxammo;	/* Force maximum */
	}
	switch(gamestate.weapon) {
	case WP_PISTOL:
	case WP_MACHINEGUN:
	case WP_CHAINGUN:
		IO_DrawAmmo(gamestate.ammo);	/* Draw the ammo */
		break;
	case WP_KNIFE:		/* Was it the knife? */
		if (gamestate.ammo == ammo) {	/* Only equip if I had NO ammo */
			gamestate.pendingweapon = WP_PISTOL;
			if (gamestate.machinegun) {
				gamestate.pendingweapon = WP_MACHINEGUN;
			}
			if (gamestate.chaingun) {		/* Use the best weapon */
				gamestate.pendingweapon = WP_CHAINGUN;
			}
		}
		break;
	default:
		break;
	}
}

/**********************************

	Award some gasoline, rearm flamethrower if knife was the current weapon

**********************************/

void GiveGas(Word ammo)
{
	gamestate.gas += ammo;		/* Add the gasoline */
	if (gamestate.gas > 99) {
		gamestate.gas = 99;		/* Maximum gas */
	}
	if (gamestate.weapon == WP_FLAMETHROWER) {	/* Flamethrower active? */
		IO_DrawAmmo(gamestate.gas);		/* Draw the gas */
	}
}

/**********************************

	Award some rockets, rearm missile launcher if knife was the current weapon

**********************************/

void GiveMissile(Word ammo)
{
	gamestate.missiles += ammo;	/* Add in the missiles */
	if (gamestate.missiles > 99) {
		gamestate.missiles = 99;	/* Maximum missiles */
	}
	if (gamestate.weapon == WP_MISSILE) {
		IO_DrawAmmo(gamestate.missiles);	/* Draw the missile count */
	}
}

/**********************************

	Award a key

**********************************/

void GiveKey(Word key)
{
	gamestate.keys |= (1<<key);		/* Add in the key mask */
	IO_DrawKeys(gamestate.keys);	/* Draw the keys */
}

/**********************************

	Play a sound for a bonus item

**********************************/

void BonusSound(void)
{
	PlaySound(SND_BONUS);
}

/**********************************

	Play a sound for getting a weapon

**********************************/

void WeaponSound(void)
{
	PlaySound(SND_GETAMMO|0x8000);
}

/**********************************

	Play a sound for getting more health

**********************************/

void HealthSound(void)
{
	PlaySound(SND_HEAL|0x8000);
}

/**********************************

	Play a sound for getting a key

**********************************/

void KeySound(void)
{
	PlaySound(SND_GETKEY);
}

/**********************************

	Picks up the bonus item at mapx,mapy. It is possible to
	have multiple items in a single tile, and they may
	not all get picked up (maxed out)

**********************************/

void GetBonus(Word x,Word y)
{
	static_t *StaticPtr;	/* Pointer to static item */
	Word Count;				/* Static count */
	Word touched;			/* Number of items touched */
	Word got;				/* Number of items taken */
	Word Truex,Truey;		/* x,y in pixels */

	Count = numstatics;/* Init the count */
	if (!Count) {	/* No items at all? */
		goto EndNow;	/* Leave NOW! */
	}
	touched = 0;		/* No items are marked */
	got = 0;			/* No items found */
	StaticPtr = statics;	/* Init the main index */
	Truex=x<<FRACBITS;		/* Convert x,y to pixel offsets */
	Truey=y<<FRACBITS;
	do {
		if (((StaticPtr->x&0xff00) != Truex) || ((StaticPtr->y&0xff00) != Truey)) {
			goto NextOne;	/* Not a tile match, skip it */
		}
		++touched;			/* Found one! */
		++got;				/* Assume I picked it up */

		switch (StaticPtr->pic) {	/* Do the proper action */
		case S_HEALTH:
			if (!gamestate.godmode && gamestate.health == 100) {
				goto KillGot;		/* No good */
			}
			HealthSound();		/* Add health */
			HealSelf(25);		/* 25 HP's better */
			break;

		case S_G_KEY:
		case S_S_KEY:
			GiveKey(StaticPtr->pic - S_G_KEY);	/* Award the key */
			KeySound();							/* Key sound */
			break;

		case S_CROSS:
		case S_CHALICE:
		case S_CHEST:
		case S_CROWN:
			BonusSound();				/* Goodie sound */
			GiveTreasure();				/* Award treasure points */
			++gamestate.treasurecount;	/* +1 treasure found */
			break;

		case S_AMMO:
			if (!gamestate.godmode && gamestate.ammo == gamestate.maxammo) {	/* Ammo full? */
				goto KillGot;			/* Abort */
			}
			WeaponSound();			/* Tah dah! */
			GiveAmmo(5);			/* 5 bullets */
			break;

		case S_MISSILES:
			if (!gamestate.godmode && gamestate.missiles == 99) {	/* Missiles full? */
				goto KillGot;		/* Abort */
			}
			WeaponSound();			/* Get ammo sound */
			GiveMissile(5);			/* Award 5 missiles */
			break;

		case S_GASCAN:
			if (!gamestate.godmode && gamestate.gas == 99) {		/* Gas full? */
				goto KillGot;		/* Abort */
			}
			WeaponSound();			/* Get ammo sound */
			GiveGas(14);			/* 14 gallons of pure death */
			break;

		case S_AMMOCASE:
			if (!gamestate.godmode && gamestate.ammo == gamestate.maxammo) {	/* Ammo full */
				goto KillGot;		/* Abort */
			}
			WeaponSound();			/* Award the bonus */
			GiveAmmo(25);			/* Give 25 bullets */
			break;

		case S_MACHINEGUN:
			if (!gamestate.godmode && gamestate.machinegun && gamestate.ammo == gamestate.maxammo) {
				goto KillGot;
			}
			GiveAmmo(6);			/* Award bullets */
			if (!gamestate.machinegun) {
				gamestate.machinegun = TRUE;	/* Make it permanent */
				GiveWeapon(WP_MACHINEGUN);		/* Arm if better */
			}
			WeaponSound();			/* Got a gun! */
			break;

		case S_CHAINGUN:
			GiveAmmo(20);
			if (!gamestate.chaingun) {
				gamestate.chaingun = TRUE;		/* Make the gun permanent */
				GiveWeapon(WP_CHAINGUN);	/* Award the chain gun */
			}
IAmSoHappy:
			PlaySound(SND_THUMBSUP);			/* Got a gun! */
			IO_DrawFace(4);			/* Make a happy face! */
			facecount = 120;			/* Hold for 2 seconds */
			break;

		case S_FLAMETHROWER:
			if (!gamestate.flamethrower) {
				gamestate.flamethrower = TRUE;	/* Make flame thrower permanent */
				GiveWeapon(WP_FLAMETHROWER);	/* Award the flame thrower */
			}
			GiveGas(20);					/* Start with 20 gallons */
			goto IAmSoHappy;				/* Yeah! */

		case S_LAUNCHER:
			if (!gamestate.missile) {
				gamestate.missile = TRUE;		/* Make missile launcher permanent */
				GiveWeapon(WP_MISSILE);			/* Award the missile launcher */
			}
			GiveMissile(5);					/* Start with 5 missiles */
			goto IAmSoHappy;				/* He he he */

		case S_ONEUP:
			HealSelf(99);					/* Fully heal */
			GiveExtraMan();					/* Give a free man */
			++gamestate.treasurecount;		/* I got a treasure */
			break;

		case S_FOOD:
			if (!gamestate.godmode && gamestate.health == 100) {	/* Full health? */
				goto KillGot;			/* Don't get it */
			}
			HealthSound();		/* Feel's good! */
			HealSelf(10);		/* Add 10 points */
			break;

		case S_DOG_FOOD:
			if (!gamestate.godmode && gamestate.health == 100) {	/* Full health? */
				goto KillGot;		/* Abort */
			}
			PlaySound(SND_DOGBARK);	/* Get a bonus */
			HealSelf(4);	/* Not much extra health */
			break;

		case S_BANDOLIER:
			if (gamestate.maxammo < 299) {	/* Maximum ammo */
				gamestate.maxammo += 100;	/* More ammo! */
			}
			WeaponSound();	/* Get a bonus */
			GiveAmmo(20);	/* Award ammo */
			GiveGas(2);
			GiveMissile(5);
			break;

		default:
			touched--;		/* Huh? */
			goto KillGot;	/* Can happen if a bonus item is dropped on a scenery pic */
		}
	/* remove the static object */

		--numstatics;		/* One less item */
		StaticPtr[0] = statics[numstatics];	/* Copy the last item */
		goto SkipInc;		/* Process this entry again! */
KillGot:
		--got;					/* Invalidate the touch */
NextOne:
		++StaticPtr;			/* Next pointer */
SkipInc:;
	} while (--Count);

	if (touched == got) {	/* All items taken? */
EndNow:
		tilemap[y][x] &= ~TI_GETABLE;		/* No more getable items */
	}
}