#include "WolfDef.h"

/**********************************

	Stop the current song from playing

**********************************/

void StopSong(void)
{
	PlaySong(0);
}

/**********************************

	Play a new song

**********************************/

void StartSong(Word Index)
{
	if (Index < SongListLen)
		PlaySong(SongListPtr[Index]);			/* Stop the previous song (If any) */
	else
		PlaySong(0);
}
