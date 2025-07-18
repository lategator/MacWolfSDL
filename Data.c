#include "WolfDef.h"
#include <stddef.h>

/**********************************

	Global data used by Wolfenstein 3-D

**********************************/

Word tilemap[MAPSIZE][MAPSIZE];		/* Main tile map */
Word ConnectCount;					/* Number of valid interconnects */
connect_t areaconnect[MAXDOORS];	/* Is this area mated with another? */
Boolean	areabyplayer[MAXAREAS];		/* Which areas can I see into? */
Word numstatics;					/* Number of active static objects */
static_t statics[MAXSTATICS];		/* Data for the static items */
Word numdoors;						/* Number of active door objects */
door_t doors[MAXDOORS];				/* Data for the door items */
Word nummissiles;					/* Number of active missiles */
missile_t missiles[MAXMISSILES];	/* Data for the missile items */
Word numactors;						/* Number of active actors */
actor_t actors[MAXACTORS];			/* Data for the actors */
Word *GameShapes[57];			/* Pointer to the game shape array */
Word difficulty;					/* 0 = easy, 1= normal, 2=hard*/
gametype_t gamestate;				/* Status of the game (Save game) */
exit_t playstate;					/* Current status of the game */
Word killx,killy;					/* X,Y of the thing that killed you! */
Boolean madenoise;					/* True when shooting or screaming*/
Boolean playermoving;				/* Is the player in motion? */
Boolean useheld;					/* Holding down the use key? */
Boolean selectheld;					/* Weapon select held down? */
Boolean attackheld;					/* Attack button held down? */
Boolean	buttonstate[NUMBUTTONS];	/* Current input */
Word joystick1;						/* Joystick value */
short joystickx=0;					/* Joystick X axis */
short joysticky=0;					/* Joystick Y axis */
int mousex;							/* Mouse x movement */
int mousey;							/* Mouse y movement */
int	mouseturn;						/* Mouse turn movement */
int mousewheel;						/* Mouse wheel */
int mousebuttons;					/* Mouse buttons */
Word nextmap;						/* Next map to warp to */
Word facecount;						/* Time to show a specific head */
Word faceframe;						/* Head pic to show */
Word elevatorx,elevatory;			/* x,y of the elevator */
Word firstframe;					/* if non 0, the screen is still faded out */
Word OldMapNum;						/* Currently loaded map # */
loadmap_t *MapPtr;					/* Pointer to current loaded map */
short clipshortangle;					/* Angle for the left edge of the screen */
short clipshortangle2;				/* clipshortangle * 2 */
Word viewx;							/* X coord of camera */
Word viewy;							/* Y coord of camera */
fixed_t	viewsin;					/* Base sine for viewing angle */
fixed_t viewcos;					/* Base cosine for viewing angle */
Word normalangle;					/* Normalized angle for view (NSEW) */
Word centerangle;					/* viewangle in fineangles*/
Word centershort;					/* viewangle in 64k angles*/
Word topspritescale;				/* Scale of topmost sprite */
Word topspritenum;					/* Shape of topmost sprite */
Word xscale[1024];	/* Scale factor for width of the screen */
Word numvisspr;				/* Number of valid visible sprites */
vissprite_t	vissprites[MAXVISSPRITES];	/* Buffer for sprite records */
vissprite_t *xevents[MAXVISSPRITES]; /* Scale events for sprite sort */
Boolean areavis[MAXAREAS];	/* Area visible */
Word bspcoord[4];			/* Rect for the BSP search */
Word TicCount;				/* Ticks since last screen draw */
Word LastTicCount;			/* Tick value at start of render */
Boolean IntermissionHack;		/* Hack for preventing double score drawing during intermission */

Word rw_maxtex;
Word rw_mintex;
LongWord rw_scalestep;
Word rw_midpoint;
Boolean rw_downside;
short rw_centerangle;
Byte *rw_texture;
LongWord rw_scale;
Byte *ArtData[64];
void *SpriteArray[S_LASTONE];
Word MacVidSize = -1;
Boolean PauseExited=FALSE;
Word SlowDown = 1;			/* Force the game to 15 hz */
Word MouseEnabled = 0;		/* Allow mouse control */
Word GameViewSize = 3;		/* Size of the game screen */
Word NoWeaponDraw=1;			/* Flag to not draw the weapon on the screen */
maplist_t *MapListPtr = NULL;		/* Pointer to map info record */
short *SoundListPtr = NULL;	/* Pointer to sound list record */
unsigned short *SongListPtr = NULL;	/* Pointer to song list record */
unsigned short *WallListPtr = NULL;	/* Pointer to wall list record */
unsigned short WallList[256];	/* List of wall numbers */
LongWord SongListLen=0;	/* Number of songs in list */
LongWord WallListLen=0;	/* Number of walls in list */
Word MaxScaler = 1;			/* Maximum number of VALID scalers */
Boolean ShowPush;			/* Cheat for pushwalls */
Byte textures[MAPSIZE*2+5][MAPSIZE];	/* Texture indexes */
Word NaziSound[] = {SND_ESEE,SND_ESEE2,SND_ESEE3,SND_ESEE4};

classinfo_t	classinfo[17] = {	/* Info for all the bad guys */
	{SND_ESEE,SND_EDIE,		/* Nazi */
	ST_GRD_WLK1, ST_GRD_STND, ST_GRD_ATK1,ST_GRD_PAIN,ST_GRD_DIE,
	100, 5, 0x0F, 6},

	{SND_ESEE,SND_EDIE,	/* Blue guard */
	ST_OFC_WLK1, ST_OFC_STND, ST_OFC_ATK1,ST_OFC_PAIN,ST_OFC_DIE,
	400, 10, 0x01, 12},

	{SND_ESEE,SND_EDIE,	/* White officer */
	ST_SS_WLK1, ST_SS_STND, ST_SS_ATK1,ST_SS_PAIN,ST_SS_DIE,
	500, 6, 0x07, 25},

	{SND_DOGBARK,SND_DOGDIE,	/* Dog */
	ST_DOG_WLK1,ST_DOG_STND,ST_DOG_ATK1,ST_DOG_WLK1,ST_DOG_DIE,
	200, 9, 0x07, 1},

	{SND_NOSOUND,SND_EDIE,		/* Mutant */
	ST_MUTANT_WLK1, ST_MUTANT_STND, ST_MUTANT_ATK1,ST_MUTANT_PAIN,ST_MUTANT_DIE,
	400, 7, 0x01, 18},

	{SND_GUTEN,SND_EDIE,			/* Hans */
	ST_HANS_WLK1, ST_HANS_STND, ST_HANS_ATK1,ST_GRD_STND,ST_HANS_DIE,
	5000,7, 0x01, 250},

	{SND_SHITHEAD,SND_EDIE,			/* Dr. Schabbs */
	ST_SCHABBS_WLK1, ST_SCHABBS_STND, ST_SCHABBS_ATK1,ST_GRD_STND,ST_SCHABBS_DIE,
	5000, 5,0x01, 350},

	{SND_GUTEN,SND_EDIE,			/* Trans */
	ST_TRANS_WLK1, ST_TRANS_STND, ST_TRANS_ATK1,ST_GRD_STND,ST_TRANS_DIE,
	5000, 7,0x01, 300},

	{SND_DOGBARK,SND_EDIE,		/* Uber knight */
	ST_UBER_WLK1, ST_UBER_STND, ST_UBER_ATK1,ST_GRD_STND,ST_UBER_DIE,
	5000, 8,0x01, 400},

	{SND_COMEHERE,SND_EDIE,			/* Dark knight */
	ST_DKNIGHT_WLK1, ST_DKNIGHT_STND, ST_DKNIGHT_ATK1,ST_GRD_STND,ST_DKNIGHT_DIE,
	5000, 7,0x01, 450},

	{SND_SHIT,SND_EDIE,			/* Mechahitler */
	ST_MHITLER_WLK1, ST_MHITLER_STND, ST_MHITLER_ATK1,ST_GRD_STND, ST_HITLER_DIE,
	5000, 7,0x01, 500},

	{SND_HITLERSEE,SND_EDIE,			/* Hitler */
	ST_HITLER_WLK1, ST_MHITLER_STND, ST_HITLER_ATK1,ST_GRD_STND,ST_HITLER_DIE,
	5000, 8,0x01, 500},

	{SND_NOSOUND,SND_EDIE,		/* Mutant */
	ST_MUTANT_WLK1, ST_MUTANT_STND, ST_MUTANT_ATK1,ST_MUTANT_PAIN,ST_MUTANT_DIE,
	400, 7, 0x01, 18},

	{SND_NOSOUND,SND_EDIE,		/* Ghost 1 */
	ST_GHOST_GREEN, ST_GHOST_GREEN, ST_GHOST_GREEN,ST_GHOST_GREEN,ST_GHOST_GREEN,
	400, 7, 0x01, 18},

	{SND_NOSOUND,SND_EDIE,		/* Ghost 2 */
	ST_GHOST_BLUE, ST_GHOST_BLUE, ST_GHOST_BLUE,ST_GHOST_BLUE,ST_GHOST_BLUE,
	400, 7, 0x01, 18},

	{SND_NOSOUND,SND_EDIE,		/* Ghost 3 */
	ST_GHOST_YELLOW, ST_GHOST_YELLOW, ST_GHOST_YELLOW,ST_GHOST_YELLOW,ST_GHOST_YELLOW,
	400, 7, 0x01, 18},

	{SND_NOSOUND,SND_EDIE,		/* Ghost 4 */
	ST_GHOST_RED, ST_GHOST_RED, ST_GHOST_RED,ST_GHOST_RED,ST_GHOST_RED,
	400, 7, 0x01, 18},
};
