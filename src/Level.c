#include "WolfDef.h"
#include <string.h>

/* static object info*/

/* List of bad guy sprites */

static Word DKnightSprs[] = {
S_DKNIGHT_ATK1,
S_DKNIGHT_ATK2,
S_DKNIGHT_ATK3,
S_DKNIGHT_ATK4,
S_DKNIGHT_WLK1,
S_DKNIGHT_WLK2,
S_DKNIGHT_WLK3,
S_DKNIGHT_WLK4,
S_DKNIGHT_DTH1,
S_DKNIGHT_DTH2,
S_DKNIGHT_DTH3,S_G_KEY,0};

static Word DogSprs[] = {
S_DOG_ATK1,
S_DOG_ATK2,
S_DOG_ATK3,
S_DOG_WLK1,
S_DOG_WLK2,
S_DOG_WLK3,
S_DOG_WLK4,
S_DOG_DTH1,
S_DOG_DTH2,
S_DOG_DTH3,0};

static Word NaziSprs[] = {
S_GUARD_ATK1,
S_GUARD_ATK2,
S_GUARD_ATK3,
S_GUARD_WLK1,
S_GUARD_WLK2,
S_GUARD_WLK3,
S_GUARD_WLK4,
S_GUARD_PAIN,
S_GUARD_DTH1,
S_GUARD_DTH2,
S_GUARD_DTH3,S_AMMO,0};

static Word HansSprs[] = {
S_HANS_ATK1,
S_HANS_ATK2,
S_HANS_ATK3,
S_HANS_WLK1,
S_HANS_WLK2,
S_HANS_WLK3,
S_HANS_WLK4,
S_HANS_DTH1,
S_HANS_DTH2,
S_HANS_DTH3,S_G_KEY,0};

static Word HitlerSprs[] = {
S_HITLER_ATK1,
S_HITLER_ATK2,
S_HITLER_ATK3,
S_HITLER_WLK1,
S_HITLER_WLK2,
S_HITLER_WLK3,
S_HITLER_WLK4,
S_HITLER_DTH1,
S_HITLER_DTH2,
S_HITLER_DTH3,
S_MHITLER_ATK1,
S_MHITLER_ATK2,
S_MHITLER_ATK3,
S_MHITLER_DIE1,
S_MHITLER_DIE2,
S_MHITLER_DIE3,
S_MHITLER_DIE4,
S_MHITLER_WLK1,
S_MHITLER_WLK2,
S_MHITLER_WLK3,
S_MHITLER_WLK4,0};

static Word UberSprs[] = {
S_UBER_ATK1,
S_UBER_ATK2,
S_UBER_ATK3,
S_UBER_ATK4,
S_UBER_WLK1,
S_UBER_WLK2,
S_UBER_WLK3,
S_UBER_WLK4,
S_UBER_DTH1,
S_UBER_DTH2,
S_UBER_DTH3,S_G_KEY,0};

static Word MutantSprs[] = {
S_MUTANT_ATK1,
S_MUTANT_ATK2,
S_MUTANT_ATK3,
S_MUTANT_WLK1,
S_MUTANT_WLK2,
S_MUTANT_WLK3,
S_MUTANT_WLK4,
S_MUTANT_PAIN,
S_MUTANT_DTH1,
S_MUTANT_DTH2,
S_MUTANT_DTH3,S_AMMO,0};

static Word OfficerSprs[] = {
S_OFFICER_ATK1,
S_OFFICER_ATK2,
S_OFFICER_ATK3,
S_OFFICER_WLK1,
S_OFFICER_WLK2,
S_OFFICER_WLK3,
S_OFFICER_WLK4,
S_OFFICER_PAIN,
S_OFFICER_DTH1,
S_OFFICER_DTH2,
S_OFFICER_DTH3,S_AMMO,0};

static Word SchabbsSpr[] = {
S_SCHABBS_ATK1,
S_SCHABBS_ATK2,
S_SCHABBS_WLK1,
S_SCHABBS_WLK2,
S_SCHABBS_WLK3,
S_SCHABBS_WLK4,
S_SCHABBS_DTH1,
S_SCHABBS_DTH2,
S_SCHABBS_DTH3,S_G_KEY,0};

static Word SSSprs[] = {
S_SS_ATK1,
S_SS_ATK2,
S_SS_ATK3,
S_SS_WLK1,
S_SS_WLK2,
S_SS_WLK3,
S_SS_WLK4,
S_SS_PAIN,
S_SS_DTH1,
S_SS_DTH2,
S_SS_DTH3,S_MACHINEGUN,S_AMMO,0};

static Word TransSprs[] = {
S_TRANS_ATK1,
S_TRANS_ATK2,
S_TRANS_ATK3,
S_TRANS_WLK1,
S_TRANS_WLK2,
S_TRANS_WLK3,
S_TRANS_WLK4,
S_TRANS_DTH1,
S_TRANS_DTH2,
S_TRANS_DTH3,S_G_KEY,0};

static Word GhostGreenSprs[] = {
	S_GHOST_GREEN,0};

static Word GhostBlueSprs[] = {
	S_GHOST_BLUE,0};

static Word GhostYellowSprs[] = {
	S_GHOST_YELLOW,0};

static Word GhostRedSprs[] = {
	S_GHOST_RED,0};

static const Word *EnemySprs[ARRAYLEN(classinfo)] = {	/* This list MUST match class_t! */
NaziSprs,
OfficerSprs,
SSSprs,
DogSprs,
MutantSprs,
HansSprs,
SchabbsSpr,
TransSprs,
UberSprs,
DKnightSprs,
HitlerSprs,
HitlerSprs,
HitlerSprs,
GhostGreenSprs,
GhostBlueSprs,
GhostYellowSprs,
GhostRedSprs,
};

static Byte EnemyHits[ARRAYLEN(EnemySprs)];

static Byte WallHits[256];

static const Word staticflags[] = {
0,					/*S_WATER_PUDDLE,*/
TI_BLOCKMOVE,		/*S_GREEN_BARREL,*/
TI_BLOCKMOVE,		/*S_CHAIR_TABLE,*/
TI_BLOCKMOVE,		/*S_FLOOR_LAMP,*/
0,					/*S_CHANDELIER,*/
TI_GETABLE,			/*S_DOG_FOOD,*/
TI_BLOCKMOVE,		/*S_COLUMN,*/
TI_BLOCKMOVE,		/*S_POTTED_TREE,*/

TI_BLOCKMOVE,		/*S_FLAG,*/
TI_BLOCKMOVE,		/*S_POTTED_PLANT,*/
TI_BLOCKMOVE,		/*S_BLUE_POT,*/
0,					/*S_DEBRIS1,*/
0,					/*S_LIGHT,*/
0,					/*S_BUCKET,*/
TI_BLOCKMOVE,		/*S_ARMOUR,*/
TI_BLOCKMOVE,		/*S_CAGE,*/

TI_GETABLE,			/*S_G_KEY,*/
TI_GETABLE,			/*S_S_KEY,*/
TI_GETABLE,			/*S_BANDOLIER*/
TI_GETABLE,			/*S_AMMOCASE,*/
TI_GETABLE,			/*S_FOOD,*/
TI_GETABLE,			/*S_HEALTH,*/
TI_GETABLE,			/*S_AMMO,*/
TI_GETABLE,			/*S_MACHINEGUN,*/

TI_GETABLE,			/*S_CHAINGUN,*/
TI_GETABLE,			/*S_CROSS,*/
TI_GETABLE,			/*S_CHALICE,*/
TI_GETABLE,			/*S_CHEST,*/
TI_GETABLE,			/*S_CROWN,*/
TI_GETABLE,			/*S_ONEUP,*/
TI_BLOCKMOVE,		/*S_WOOD_BARREL,*/
TI_BLOCKMOVE,		/*S_WATER_WELL,*/
TI_GETABLE,			/*S_FLAMETHROWER */
TI_GETABLE,			/*S_GASCAN */
TI_GETABLE,			/*S_LAUNCHER */
TI_GETABLE,			/*S_MISSILES */
0,					/*S_GUARD_DTH3*/
};

/**********************************

	Spawn a static object at x,y

**********************************/

void SpawnStatic(Word x,Word y,Word shape)
{
	Word *TilePtr;
	static_t *StatPtr;

	if (numstatics>=MAXSTATICS || shape >= ARRAYLEN(staticflags)) {
		return;				/* Oh oh!! */
	}
	TilePtr = &tilemap[y][x];		/* Precalc tile pointer */
	StatPtr = &statics[numstatics];
	StatPtr->x = (x<<FRACBITS)+0x80;	/* Set the pixel X */
	StatPtr->y = (y<<FRACBITS)+0x80;	/* Set the pixel Y */
	StatPtr->areanumber = TilePtr[0] & TI_NUMMASK;	/* Mask off the area number */
	TilePtr[0] |= staticflags[shape];
	if (shape == 36)
		shape = S_GUARD_DTH3;
	else
		shape += S_WATER_PUDDLE;	/* Init the shape number */
	WallHits[shape] = 1;		/* Load in this shape */
	StatPtr->pic = shape;		/* Set the object's shape */
	switch (shape) {
	case S_CROSS:
	case S_CHALICE:
	case S_CHEST:
	case S_CROWN:
	case S_ONEUP:
		++gamestate.treasuretotal;	/* Mark this as treasure */
	}
	++numstatics;				/* I have one more item */
}

/**********************************

	Spawn the player at x,y

**********************************/

void SpawnPlayer(Word x,Word y,Word dir)
{
	gamestate.viewangle = (1-dir)*ANGLES/4;	/* Get the basic viewing angle */
	actors[0].x = (x<<FRACBITS)+0x80;	/* Save the X coord */
	actors[0].y = (y<<FRACBITS)+0x80;	/* Save the Y coord */
	actors[0].class = CL_PLAYER;	/* This is the player */
	actors[0].areanumber = tilemap[y][x]&TI_NUMMASK;	/* Which area? */
	ConnectCount = 0;		/* Zap the interconnects */
	ConnectAreas();			/* Init the area table */
}

/**********************************

	Spawn an actor at a specific x,y

**********************************/

actor_t *SpawnStand(Word x,Word y,class_t which)
{
	stateindex_t state;
	classinfo_t	*info;
	actor_t *ActorPtr;
	Word *TilePtr;
	Word tile;

	if (numactors==MAXACTORS || which >= ARRAYLEN(classinfo)) {	/* Too many actors already? */
		return NULL;					/* Exit */
	}

	EnemyHits[which] = 1;
	info = &classinfo[which];	/* Get the pointer to the basic info */
	state = info->standstate;	/* Get the state logic value */
	ActorPtr = &actors[numactors];	/* Get the pointer to the new actor entry */
	TilePtr = &tilemap[y][x];	/* Pointer to the current tile */
	tile = TilePtr[0];			/* What's in the tile? */

	ActorPtr->x = (x<<FRACBITS)+0x80;	/* Init the x and y */
	ActorPtr->y = (y<<FRACBITS)+0x80;
	ActorPtr->pic = states[state].shapenum;	/* What picture to display? */
	ActorPtr->ticcount = states[state].tictime;	/* Initial tick count */
	ActorPtr->state = state;
	if (which < CL_GHOST_GREEN || which > CL_GHOST_RED) {
		++gamestate.killtotal;		/* Another critter must die! */
		ActorPtr->flags = FL_SHOOTABLE | FL_NOTMOVING;	/* You can shoot it */
	} else {
		ActorPtr->flags = 0;
	}
	ActorPtr->distance = 0;			/* No distance to travel */
	ActorPtr->dir = nodir;				/* No direction of travel */
	ActorPtr->areanumber = tile&TI_NUMMASK;	/* Area in */
	ActorPtr->reacttime = 0;	/* Reaction time */
	TilePtr[0] = tile | TI_ACTOR;	/* Mark as a bad guy */
	MapPtr->tilemap[y][x] = numactors;	/* Save the actor # */
	ActorPtr->goalx = x;		/* Don't travel anywhere */
	ActorPtr->goaly = y;
	ActorPtr->class = which;	/* Actor type */
	ActorPtr->speed = info->speed;	/* Basic speed */
	ActorPtr->hitpoints = info->hitpoints;	/* Starting hit points */
	++numactors;	/* I now add one more actor to the list */

	return ActorPtr;
}


/**********************************

	Spawn an actor at a specific x,y and ALSO set the ambush flag

**********************************/

void SpawnAmbush(Word x,Word y,class_t which)
{
	actor_t *ActorPtr;

	ActorPtr = SpawnStand(x,y,which);		/* Fill in all the entries */
	if (!ActorPtr)
		return;
	ActorPtr->flags |= FL_AMBUSH;	/* Set the ambush flag */
}

/**********************************

	Spawn a door at the specified x,y

**********************************/

void SpawnDoor(Word x,Word y,Word type)
{
	door_t *door;
	Word *TilePtr;

	if (numdoors==MAXDOORS) {
		return;
	}

	TilePtr = &tilemap[y-1][x];	/* Pointer to the tile (-1 y) for door->area index */
	door = &doors[numdoors];	/* Pointer to the door record */

	door->position = 0;		/* doors start out fully closed*/
	door->tilex	= x;
	door->tiley	= y;
	door->info	= type-90;	/* Set the door type */
	door->action = DR_CLOSED;	/* Mark as closed */
	TilePtr[MAPSIZE] = (TI_DOOR|TI_BLOCKMOVE|TI_BLOCKSIGHT)| numdoors;

	if (type & 1) {	/* horizontal*/
		door->area1 = TilePtr[MAPSIZE*2] & TI_NUMMASK;	/* One cell below */
		door->area2 = TilePtr[0] & TI_NUMMASK;			/* One cell above */
	} else {
		door->area1 = TilePtr[MAPSIZE-1] & TI_NUMMASK;	/* One cell left */
		door->area2 = TilePtr[MAPSIZE+1] & TI_NUMMASK;	/* One cell right */
	}
	++numdoors;
}

/**********************************

	Spawn a pushwall track at the specified x,y

**********************************/

void AddPWallTrack(Word x,Word y,Word tile)
{
	Byte *TextPtr;
	if (x>=MAPSIZE || y>=MAPSIZE) {
		return;		/* pushwall is off the edge of a map*/
	}
	TextPtr = &textures[MAPSIZE+x][y];
	if (TextPtr[0] != 0xff) {	/* Already marked? */
		return;					/* solid wall*/
	}
	TextPtr[0] = tile + 1;		/* Save the tile # */
	textures[y][x] = tile;		/* Mark the texture index */
}

/**********************************

	Spawn a pushwall at the specified x,y

**********************************/

void SpawnPushwall(Word x,Word y,Word tile)
{
	++gamestate.secrettotal;		/* Secret area! */
	tilemap[y][x] |= (TI_BLOCKMOVE | TI_BLOCKSIGHT | TI_PUSHWALL);
/* directly set texture values for rendering*/
	tile = (tile-1)<<1;
	AddPWallTrack(x, y, tile);
	AddPWallTrack(x-1, y, tile);
	AddPWallTrack(x-2, y, tile);
	AddPWallTrack(x+1, y, tile);
	AddPWallTrack(x+2, y, tile);
	AddPWallTrack(x, y-1, tile);
	AddPWallTrack(x, y-2, tile);
	AddPWallTrack(x, y+1, tile);
	AddPWallTrack(x, y+2, tile);
}

/**********************************

	Spawn an elevator at the specified x,y

**********************************/

void SpawnElevator(Word x,Word y)
{
	elevatorx = x;	/* for easy mode cheating*/
	elevatory = y;
	tilemap[y][x] |= TI_BLOCKMOVE | TI_SWITCH;
}

/**********************************

	Spawn the outside at the specified x,y

**********************************/

void SpawnOut(Word x,Word y)
{
	/*tilemap[y][x] |= TI_BLOCKMOVE | TI_SWITCH;*/
}

/**********************************

	Spawn a secret item at the specified x,y

**********************************/

void SpawnSecret(Word x,Word y)
{
	tilemap[y][x] |= TI_BLOCKMOVE | TI_SECRET;
}

/**********************************

	Spawn all actors and mark down special places
	A spawn record is (x,y,type)

**********************************/

void SpawnThings(void)
{
	Word x,y;		/* Temp x,y */
	Word type;		/* Item to spawn */
	Byte *spawn_p;
	Word Count;		/* Number of items to create */
	const Word *EnemyPtr;

	memset(EnemyHits,0,sizeof(EnemyHits));
	spawn_p = (Byte *)MapPtr+MapPtr->spawnlistofs;	/* Point to the spawn table */
	Count = MapPtr->numspawn;		/* How many items to spawn? */
	if (!Count) {
		return;
	}
	do {
		x = spawn_p[0];		/* Get the X */
		y = spawn_p[1];		/* Get the y */
		type = spawn_p[2];	/* Get the type */
		spawn_p+=3;			/* Index past the record */
		if (type<19) {
			continue;
		} else if (type<23) {	/* 19-22 */
			SpawnPlayer(x,y,type-19);
		} else if (type<60) {	/* 23-59 */
			SpawnStatic(x,y,type-23);
		} else if (type<90) {	/* 60-89 */
			continue;
		} else if (type<98) {	/* 90-97 */
			SpawnDoor(x,y,type);
		} else if (type == 98) {	/* 98 */
			SpawnPushwall(x,y,spawn_p[0]);
			++spawn_p;
		} else if (type == 99) {	/* 99 */
			SpawnOut(x,y);
		} else if (type == 100) {	/* 100 */
			SpawnElevator(x,y);
		} else if (type == 101) {	/* 101 */
			SpawnSecret(x,y);
		} else if (type<108) {		/* 102-107 */
			continue;
		} else if (type<126) {		/* 108-125 */
			SpawnStand(x,y,(class_t) (type-108));
		} else if (type<140) {		/* 126-139 */
			SpawnAmbush(x,y,(class_t) (type-126));
		}
	} while (--Count);
	Count = 0;
	do {
		if (EnemyHits[Count]) {
			x = 0;
			EnemyPtr = EnemySprs[Count];
			do {
				WallHits[EnemyPtr[x]] = 1;
				++x;
			} while (EnemyPtr[x]);
		}
	} while (++Count<ARRAYLEN(EnemySprs));
}

/**********************************

	Release the map data

**********************************/

void ReleaseMap(void)
{
	Word i;
	if (OldMapNum) {
		KillAResource(OldMapNum);	/* Make SURE it's purged! */
		OldMapNum = 0;			/* Nothing loaded */
		MapPtr = 0;				/* No data available */
	}
	i = 0;
	do {
		if (ArtData[i]) {
			FreeSomeMem(ArtData[i]);	/* Release the wall art */
			ArtData[i] = 0;
		}
	} while (++i<64);				/* All walls done? */
	i = 1;
	do {
		if (SpriteArray[i]) {			/* Is this sprite loaded? */
			FreeSomeMem(SpriteArray[i]);	/* Release the memory */
			SpriteArray[i] = 0;
		}
	} while (++i<S_LASTONE);			/* All done? */
}

/**********************************

	Load and initialize everything for current game level

	Spawnlist 0-(numusable-1) are the usable (door/pushwall) spawn objects
	Polysegs 0-(numpushwalls-1) are four sided pushwall segs

**********************************/

Boolean SetupGameLevel(void)
{
	Byte *src;
	Word *dest;
	Word tile;
	Word Count;
	LongWord Length;

/* clear counts*/

	gamestate.secrettotal=0;		/* No secret items found */
	gamestate.killtotal=0;			/* Noone killed */
	gamestate.treasuretotal=0;		/* No treasure found */
	gamestate.secretcount=0;		/* No secret areas found */
	gamestate.killcount=0;			/* Noone to kill on this level */
	gamestate.treasurecount = 0;	/* No treasures laid */

/* Load a level */

	ReleaseMap();				/* Free up any previous map */
	if (!MapListPtr)
		BailOut("No levels found in scenario file");
	OldMapNum = (MapListPtr->MapRezNum)+gamestate.mapon;	/* Which map to load */
	Length = ResourceLength(OldMapNum);
	if (Length < sizeof *MapPtr)
		return FALSE;
	MapPtr = LoadAResource(OldMapNum);	/* Load in the map */
	if (!MapPtr)
		return FALSE;		/* Uh.. yeah... */
	DrawPsyched(1);		/* First stage done */
	MapPtr->numspawn = SwapUShortLE(MapPtr->numspawn);	/* Fix for 68000 machines */
	MapPtr->spawnlistofs = SwapUShortLE(MapPtr->spawnlistofs);
	MapPtr->numnodes = SwapUShortLE(MapPtr->numnodes);
	MapPtr->nodelistofs = SwapUShortLE(MapPtr->nodelistofs);
	if (Length < MapPtr->spawnlistofs + MapPtr->numspawn * 3 || Length < MapPtr->nodelistofs + MapPtr->numnodes * sizeof(savenode_t)) {
		ReleaseAResource(OldMapNum);
		return FALSE;
	}
	numstatics = 0;		/* Clear out the static array */
	numdoors = 0;		/* Clear out the door array */
	numactors = 1;		/* player has spot 0*/
	nummissiles = 0;	/* Clear out the missile array */

/* expand the byte width map to word width*/

	memset(WallHits,0,sizeof(WallHits));
	src = &MapPtr->tilemap[0][0];
	dest = &tilemap[0][0];
	Count = 0;
	do {
		tile = src[Count];		/* Get the byte tile */
		if (tile & TI_BLOCKMOVE) {
			tile |= TI_BLOCKSIGHT;	/* Mark as blocking my sight */
			WallHits[(tile-1)&0x1F]=1;
		}
		dest[Count] = tile;		/* Save the new tile */
	} while (++Count<MAPSIZE*MAPSIZE);


/* let the rendering engine know about the new map*/

	NewMap();		/* Set the variables */
	DrawPsyched(2);	/* Second stage */
	if (!LoadWallArt()) {	/* Get the wall shapes */
		return FALSE;
	}
/* map.tilemap is now used to hold actor numbers if the TI_ACTOR bit is set in
	the word tilemap*/

	memset(WallHits,0,sizeof(WallHits));	/* Init the wall table */
	Count = 1;
	do {
		WallHits[Count] = 1;	/* Force the permanent sprites to load */
	} while (++Count<S_VICTORY+1);
	SpawnThings();	/* Create all the characters in the game level */
	return LoadSpriteArt();	/* Load in the sprite art */
}

/************************************

	Load in a single wall shape

************************************/

static Boolean LoadWallShape(Word Index,const Byte *restrict DarkPtr)
{
	Byte *WallPtr;
	Byte *Buffer;
	Word WallVal;
	Word WallNum;
	Word j;

	if (Index+1 >= WallListLen)
		return FALSE;
	Buffer = AllocSomeMem(0x4000);		/* Get memory for wall */
	if (!Buffer)
		return FALSE;
	WallVal = WallList[Index+1];		/* Which resource is it? */
	WallNum = WallVal&0x3fff;
	if (!WallNum)
		return FALSE;
	WallPtr = LoadAResource(WallNum);	/* Load the shape */
	if (!WallPtr) {
		FreeSomeMem(Buffer);
		return FALSE;
	}
	DLZSS(Buffer,0x4000,WallPtr, ResourceLength(WallNum));	/* Decompress it */
	if (DarkPtr && (WallVal & 0x8000)) {		/* Do I need to darken it? */
		j = 0;
		do {
			Buffer[j] = DarkPtr[Buffer[j]];	/* Use a lookup table to darken it */
		} while (++j<0x4000);
	}
	ArtData[Index] = Buffer;			/* Save the pointer */
	ReleaseAResource(WallNum);	/* Purge the data */
	return TRUE;
}

/************************************

	Load in all the wall art

************************************/

Word LoadWallArt(void)
{
	Word i;
	Word j;
	Word RetVal;
	Byte *DarkPtr = NULL;

	RetVal = FALSE;
	if (ResourceLength(MyDarkData) >= 256)
		DarkPtr = LoadAResource(MyDarkData);		/* Get my darken table */
	i = 0;
	do {
		if (WallHits[i]) {
			j = i*2;
			if (!LoadWallShape(j,DarkPtr) ||
				!LoadWallShape(j+1,DarkPtr) ) {
				goto Abort;
			}
			DrawPsyched(j+2);
		}
	} while (++i<29);
	i = 59;
	do {
		if (!LoadWallShape(i,DarkPtr)) {
			goto Abort;
		}
		DrawPsyched(i+2);
	} while (++i<64);
	RetVal = TRUE;
Abort:
	ReleaseAResource(MyDarkData);
	return RetVal;			/* No errors */
}

/***************************

	Load in all the sprites

***************************/

Word LoadSpriteArt(void)
{
	Word i,j;
	Word Length;
	Byte *PackPtr;
	Word *CharPtr;
	const SpriteRun *RunPtr;
	Word Num;
	Word Width;
	Word Offset;
	LongWord Size;

	i=1;
	do {
		SpriteArray[i] = NULL;
		if (WallHits[i]) {
			Num = i+(428-1);
			CharPtr = NULL;
			PackPtr = LoadAResource(Num);	/* Get the packed file */
			if (!PackPtr)
				goto Next;
			Size = ResourceLength(Num);
			if (Size < 2)
				goto Next;
			Length = PackPtr[0];				/* Get the length unpacked */
			Length |= PackPtr[1]<<8;
			if (Length < 2)
				goto Next;
			CharPtr = AllocSomeMem(Length);		/* Get memory for the sprite */
			DLZSS((Byte*)CharPtr,Length,&PackPtr[2], Size - 2);	/* Unpack it */
			Width = SwapUShortBE(CharPtr[0]);
			if (Length < Width*2+2)
				goto Next;
			for (j = 0; j < Width; j++) {		/* Bounds check all sprite columns */
				Offset = SwapUShortBE(CharPtr[j+1]);
				if (Length < Offset + sizeof(SpriteRun))
					goto Next;
				RunPtr = (const void*) &CharPtr[Offset/2];
				while (SwapUShortBE(RunPtr->Topy) != (unsigned short) -1) {		/* Not end of record? */
					if (Length < SwapUShortBE(RunPtr->Shape)+SwapUShortBE(RunPtr->Boty)/2-SwapUShortBE(RunPtr->Topy)/2)
						goto Next;
					RunPtr++;
				}
			}
			SpriteArray[i] = CharPtr;		/* Save the pointer */
			CharPtr = NULL;
		Next:
			if (CharPtr)
				FreeSomeMem(CharPtr);
			ReleaseAResource(Num);		/* Release the resource */
			DrawPsyched(i+66);
		}
	} while (++i<S_LASTONE);
	DrawPsyched(66+S_LASTONE);
	return TRUE;
}
