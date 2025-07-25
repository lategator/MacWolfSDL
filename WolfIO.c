#include "WolfDef.h"
#include <string.h>

/**********************************

	Draw a space padded list of numbers
	for the score

**********************************/

LongWord pow_10[] = {1,10,100,1000,10000,100000,1000000};
Word NumberIndex = 36;		/* First number in the shape list... */

void SetNumber(LongWord number,Word x,Word y,Word digits)
{
    LongWord val;
    Word count;
    Word empty;

    empty = 1;			/* No char's drawn yet */
    while (digits) {	/* Any digits left? */
         count = 0;		/* No value yet */
         val = pow_10[digits-1];	/* Get the power of 10 */
         while (number >= val) {	/* Any value here? */
             count++;		/* +1 to the count */
             number -= val;		/* Remove the value */
         }
         if (empty && !count && digits!=1) {    /* pad on left with blanks rather than 0 */
              DrawShape(x,y,GameShapes[NumberIndex]);
         } else {
              empty = 0;		/* I have drawn... */
              DrawShape(x,y,GameShapes[count+NumberIndex]);	/* Draw the digit */
         }
         x+=ScaleX(8);
         digits--;		/* Count down */
    }
}

/**********************************

	Read from the Mac's keyboard/mouse system

**********************************/

exit_t IO_CheckInput(void)
{
	exit_t PS;

	PS = ReadSystemJoystick();	/* Set the variable "joystick1" */
	if (PS)
		return PS;

	/* check for auto map */

	if (joystick1 & JOYPAD_START) {
		PS = RunAutoMap();		/* Do the auto map */
		if (PS)
			return PS;
	}

/*
** get game control flags from joypad
*/
    memset(buttonstate,0,sizeof(buttonstate));	/* Zap the buttonstates */
    if (joystick1 & JOYPAD_UP)
         buttonstate[bt_north] = 1;
    if (joystick1 & JOYPAD_DN)
         buttonstate[bt_south] = 1;
    if (joystick1 & JOYPAD_LFT)
         buttonstate[bt_west] = 1;
    if (joystick1 & JOYPAD_RGT)
         buttonstate[bt_east] = 1;
    if (joystick1 & JOYPAD_TL)
         buttonstate[bt_left] = 1;
    if (joystick1 & JOYPAD_TR)
         buttonstate[bt_right] = 1;
    if (joystick1 & JOYPAD_B)
         buttonstate[bt_attack] = 1;
    if (joystick1 & (JOYPAD_Y|JOYPAD_X) )
         buttonstate[bt_run] = 1;
    if (joystick1 & JOYPAD_A)
         buttonstate[bt_use] = 1;
    if (joystick1 & JOYPAD_SELECT) {
         buttonstate[bt_select] = 1;
	}
	return 0;
}

/**********************************

	Draw the floor and castle #

**********************************/

void IO_DrawFloor(Word floor)
{
    SetNumber(MapListPtr->InfoArray[floor].ScenarioNum,ScaleX(8),ScaleY(176),1);
    SetNumber(MapListPtr->InfoArray[floor].FloorNum,ScaleX(32),ScaleY(176),1);
}

/**********************************

	Draw the score

**********************************/

void IO_DrawScore(LongWord score)
{
	if (!IntermissionHack) {			/* Don't draw during intermission! */
    	SetNumber(score,ScaleX(56),ScaleY(176),7);
    }
}

/**********************************

	Draw the number of live remaining

**********************************/

void IO_DrawLives(Word lives)
{

   	if (!IntermissionHack) {			/* Don't draw during intermission! */
	    --lives;			/* Adjust for zero start value */
    	if (lives > 9) {
    		lives = 9;		/* Failsafe */
		}
		SetNumber(lives,ScaleX(188),ScaleY(176),1);		/* Draw the lives count */
	}
}

/**********************************

	Draw the health

**********************************/

void IO_DrawHealth(Word health)
{
    SetNumber(health,ScaleX(210),ScaleY(176),3);
}

/**********************************

	Draw the ammo remaining

**********************************/

void IO_DrawAmmo(Word ammo)
{
    SetNumber(ammo,ScaleX(268),ScaleY(176),3);
}

/**********************************

	Draw the treasure score

**********************************/

void IO_DrawTreasure(Word treasure)
{
    SetNumber(treasure,ScaleX(128),ScaleY(176),2);
}

/**********************************

	Draw the keys held

**********************************/

void IO_DrawKeys(Word keys)
{
    if (keys&1) {
         DrawShape(ScaleX(310),ScaleY(164),GameShapes[10]);
    }
    if (keys&2) {
    	DrawShape(ScaleX(310),ScaleY(184),GameShapes[11]);
    }
}

/**********************************

	Draw the gun in the foreground

**********************************/

void IO_AttackShape(Word shape)
{
	DrawXMShape(ScaleX(128),ScaleY(96),GameShapes[shape+12]);
}

/**********************************

	Draw the BJ's face

**********************************/

void IO_DrawFace(Word face)
{
	DrawShape(ScaleX(160),ScaleY(164),GameShapes[face]);		/* Draw the face */
}

/**********************************

	Redraw the main status bar

**********************************/

void IO_DrawStatusBar(void)
{
	DrawShape(ScaleX(0),ScaleY(160),GameShapes[46]);
}

/**********************************

	Erase the floor and ceiling

**********************************/

#ifndef __APPLEIIGS__		/* Done in assembly on the IIgs version */
#ifndef __3DO__
void IO_ClearViewBuffer(void)
{
	unsigned char *Screenad;
	Word Count,WCount;
	LongWord *LScreenad;
	LongWord Fill;

	Screenad = VideoPointer;
	Count = VIEWHEIGHT/2;
	Fill = 0x2f2f2f2f;
	do {
		WCount = SCREENWIDTH/4;
		LScreenad = (LongWord *) Screenad;
		do {
			*LScreenad++ = Fill;	/* 004 */
		} while (--WCount);
		Screenad+=VideoWidth;
	} while (--Count);
	Count = VIEWHEIGHT/2;
	Fill = 0x2A2A2A2A;
	do {
		WCount = SCREENWIDTH/4;
		LScreenad = (LongWord *) Screenad;
		do {
			*LScreenad++ = Fill;
		} while (--WCount);
		Screenad+=VideoWidth;
	} while (--Count);
}
#endif
#endif

/**********************************

	Copy the 3-D screen to display memory

**********************************/

void IO_DisplayViewBuffer (void)
{
	BlastScreen();
/* if this is the first frame rendered, upload everything and fade in */
    if (firstframe) {
		FadeTo(rGamePal);
		firstframe = 0;
    }
}
