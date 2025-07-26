#pragma once

#include <stdint.h>
#ifdef _WIN32
#include <intrin.h>
#endif

#ifdef _MSC_VER
#define __attribute__(...)
#define __builtin_unreachable(...) (__assume(false))
#define __builtin_strlen strlen
#define __builtin_bswap16 _byteswap_ushort
#define __builtin_bswap32 _byteswap_ulong
#endif

typedef uint16_t Word;
typedef uint32_t LongWord;
typedef uint8_t Byte;
typedef uint8_t Boolean;

struct Rect {
  short               top;
  short               left;
  short               bottom;
  short               right;
};
typedef struct Rect                     Rect;

#define BLACK 255
#define DARKGREY 250
#define BROWN 101
#define PURPLE 133
#define BLUE 210
#define DARKGREEN 229
#define ORANGE 23
#define RED 216
#define BEIGE 14
#define YELLOW 5
#define GREEN 225
#define LIGHTBLUE 150
#define LILAC 48
#define PERIWINKLE 120
#define LIGHTGREY 43
#define WHITE 0

#define SfxActive 1
#define MusicActive 2

#define VideoSize 64000
#define SetAuxType(x,y)
#define SetFileType(x,y)

extern unsigned char *VideoPointer;
//extern Word ScanCode;
extern Word KilledSong;
extern Word SystemState;
extern Boolean FullScreen;
extern Byte ScreenScaleMode;
extern Boolean ScreenFilter;
extern Word VideoWidth;
extern LongWord LastTick;
extern LongWord YTable[480];

void DLZSS(Byte *restrict Dest,LongWord DstLen,const Byte *restrict Src,LongWord SrcLen);

#define ARRAYLEN(a) (sizeof((a)) / sizeof((a)[0]))

#if !defined(_MSC_VER) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SwapLongLE(__x) __builtin_bswap32(__x)
#define SwapUShortLE(__x) __builtin_bswap16(__x)
#define SwapShortLE(__x) ((int16_t)(__builtin_bswap16(__x)))
#define SwapLongBE(__x) ((uint32_t)(__x))
#define SwapUShortBE(__x) ((uint16_t)(__x))
#define SwapShortBE(__x) ((int16_t)(__x))
#else
#define SwapLongLE(__x) ((uint32_t)(__x))
#define SwapUShortLE(__x) ((uint16_t)(__x))
#define SwapShortLE(__x) ((int16_t)(__x))
#define SwapLongBE(__x) __builtin_bswap32(__x)
#define SwapUShortBE(__x) __builtin_bswap16(__x)
#define SwapShortBE(__x) ((int16_t)(__builtin_bswap16(__x)))
#endif

void WaitTick(void);
void WaitTicks(Word TickCount);
int WaitTicksEvent(Word TickCount);
Word WaitEvent(void);
LongWord ReadTick(void);
void *AllocSomeMem(LongWord Size);
void FreeSomeMem(void *MemPtr);
char *AllocFormatStr(const char *Fmt, ...) __attribute__((format(printf, 1, 2)));

void SoundOff(void);
void PlaySound(Word SndNum);
void StopSound(Word SndNum);
void PlaySong(Word SongNum);

void ClearTheScreen(Word Color);
void ShowPic(Word PicNum);

void InitYTable(void);
void InstallAFont(Word FontNum);
void FontUseMask(void);
void FontUseZero(void);
void SetFontXY(Word x,Word y);
void FontSetClip(const Rect *Rect);
void FontSetColor(Word Color);
void DrawAString(const char *TextPtr);
void DrawAChar(LongWord CodePoint);
Word GetRandom(Word Range);
void Randomize(void);
void DrawShape(Word x,Word y,void *ShapePtr);
void DrawXMShape(Word x,Word y,void *ShapePtr);
void DrawMShape(Word x,Word y,void *ShapePtr);
void EraseMBShape(Word x,Word y, void *ShapePtr,void *BackPtr);
Word TestMShape(Word x,Word y,void *ShapePtr);
Word TestMBShape(Word x,Word y,void *ShapePtr,void *BackPtr);

void SetAPalette(Word PalNum);
void SetAPalettePtr(unsigned char *PalPtr);
void FadeTo(Word PalNum);
void FadeToBlack(void);
void FadeToPtr(unsigned char *PalPtr);

void RegisterSounds(short *SoundIDs, LongWord Len);
void InitResources(void);
void KillResources(void);
Boolean MountMapFile(const char *FileName);
void *LoadCompressed(Word RezNum, LongWord *Length);
void *LoadCompressedShape(Word RezNum);
void *LoadAResource(Word RezNum);
LongWord ResourceLength(Word RezNum);
void ReleaseAResource(Word RezNum);
void KillAResource(Word RezNum);

typedef struct {
	Word ID;
	Word samplerate;
	uint8_t basenote;
	int loopstart;
	int loopend;
	LongWord offset;
	LongWord size;
	void *data;
} Sound;

Sound *LoadCachedSound(Word RezNum);
Byte *LoadSong(Word RezNum, LongWord *Len);
Boolean LoadInstrument(LongWord Index, Byte *Buf);
